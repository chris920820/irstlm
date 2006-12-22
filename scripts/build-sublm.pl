#! /usr/bin/perl

#first pass: read full input and generate 1-grams
#second pass: 
#for n=2 to N
#  foreach n-1-grams
#      foreach  n-grams with history n-1
#          compute smoothing statistics
#          store successors
#      compute back-off probability
#      compute smoothing probability
#      write n-1 gram with back-off prob 
#      write all n-grams with smoothed probability

use strict;
use Getopt::Long "GetOptions";

my $gzip="/usr/bin/gzip";
my $gunzip="/usr/bin/gunzip";


my($help,$size,$freqshift,$ngrams,$sublm,$witten_bell,$kneser_ney)=();

$help=1 unless
&GetOptions('size=i' => \$size,
            'freq-shift=i' => \$freqshift, 
             'ngrams=s' => \$ngrams,
             'sublm=s' => \$sublm,
             'witten-bell' => \$witten_bell,
             'kneser-ney' => \$kneser_ney,
             'help' => \$help,);


if ($help || !$size || !$ngrams || !$sublm){
  print "build-sublm.pl <options>\n",
        "--size <int>        maximum n-gram size for the language model\n",
        "--ngrams <string>   input file or command to read the ngram table\n",
        "--sublm <string>    output file prefix to write the sublm statistics \n",
        "--freq-shift <int>  (optional) value to be subtracted from all frequencies\n",
        "--kneser-ney         use kneser-ney smoothing\n",
        "--witten-bell        (default)use witten bell smoothin\n",
        "--help              (optional) print these instructions\n";    
  exit(1);
}

$witten_bell++ if !$witten_bell && !$kneser_ney;

die "build-sublm: value of --size must be larger than 0\n" if $size<1;
die "build-sublm: choose one smoothing method\n" if $witten_bell && $kneser_ney;

my $log10=log(10.0);  #service variable to convert log into log10

my $oldwrd="";      #variable to check if 1-gram changed 

my @cnt=();         #counter of n-grams
my $totcnt=0;       #total counter of n-grams
my ($ng,@ng);       #read ngrams
my $ngcnt=0;        #store ngram frequency

warn "Collecting 1-gram counts\n";

open(INP,"$ngrams") || open(INP,"$ngrams|")  || die "cannot open $ngrams\n";
open(GR,"|$gzip -c >${sublm}.1gr.gz") || die "cannot create ${sublm}.1gr.gz\n";

while ($ng=<INP>){
  
  chop $ng; @ng=split(/[ \t]/,$ng); $ngcnt=(pop @ng) - $freqshift;
  
  if ($oldwrd ne $ng[0]){
    printf (GR "%s %s\n",$totcnt,$oldwrd) if $oldwrd ne '';
    $totcnt=0;$oldwrd=$ng[0];
  }
  
  #update counter
  $totcnt+=$ngcnt;
}

printf GR "%s %s\n",$totcnt,$oldwrd;
close(INP);
close(GR);

my (@h,$h,$hpr);   #n-gram history 
my (@dict,$code);  #sorted dictionary of history successors
my $diff;          #different successors of history
my $locfreq;       #accumulate frequency of n-grams of given size
my ($N1,$N2,$beta);   #Kneser-Ney Smoothing: n-grams occurring once or twice 

warn "Computing n-gram probabilities:\n"; 

foreach (my $n=2;$n<=$size;$n++){
  
  warn "$n-grams\n";
  open(HGR,"$gunzip -c ${sublm}.".($n-1)."gr.gz|") || die "cannot open ${sublm}.".($n-1)."gr.gz\n";
  open(INP,"$ngrams") || open(INP,"$ngrams|")  || die "cannot open $ngrams\n";
  open(GR,"|$gzip -c >${sublm}.${n}gr.gz");
  open(NHGR,"|$gzip -c > ${sublm}.".($n-1)."ngr.gz") || die "cannot open ${sublm}.".($n-1)."ngr.gz";

  chop($ng=<INP>); @ng=split(/[ \t]/,$ng);$ngcnt=(pop @ng) - $freqshift;
  chop($h=<HGR>);  @h=split(/ /,$h); $hpr=shift @h;
  
  $code=-1;@cnt=(); @dict=(); $totcnt=0;$diff=0; $oldwrd="";$N1=0;$N2=0;$locfreq=0;
   
  do{
    while (join(" ",@h[0..$n-2]) eq join(" ",@ng[0..$n-2])){ #true the first time
        #print join(" ",@h[0..$n-2])," --- ", join(" ",@ng[0..$n-2])," $ngcnt \n";    

        #collect smoothing statistics 
      if ($oldwrd ne $ng[$n-1]){
        $dict[++$code]=$oldwrd=$ng[$n-1];
        $diff++;
        $N1++ if $locfreq==1;
        $N2++ if $locfreq==2;
        $locfreq=$ngcnt;
      }
      else{
          $locfreq+=$ngcnt;
      }
            
      $cnt[$code]+=$ngcnt; $totcnt+=$ngcnt;           
 
      #read next ngram
        chop($ng=<INP>); @ng=split(/[ \t]/,$ng);$ngcnt=(pop @ng) - $freqshift;	
      }

      #Kneser-Ney Smoothing 
      
      if ($kneser_ney){
        if ($N1==0 || $N2==0){
          warn "Error in Kneser-Ney smoothing N1 $N1 N2 $N2 diff $diff: resorting to Witten-Bell\n";
          $beta=0;  
        }
        else{
          $beta=$N1/($N1 + 2 * $N2); 
        }
      }
      
      #print smoothed probabilities
     
      for (my $c=0;$c<=$code;$c++){ 
        if ($kneser_ney && $beta>0){
          printf GR "%f %s %s\n",log(($cnt[$c]-$beta)/$totcnt)/$log10,
                    join(" ",@h[0..$n-2]),$dict[$c];
        }else{
            printf GR "%f %s %s\n",log($cnt[$c]/($totcnt+$diff))/$log10,
          join(" ",@h[0..$n-2]),$dict[$c];
        }
      }

     #rewrite history including back-off weight
      
     print "$h - $ng - $totcnt $diff \n" if $totcnt+$diff==0;
     if ($kneser_ney && $beta>0){
       printf NHGR "%s %f\n",$h,log($beta * ($diff-$N1)/$totcnt)/$log10;
     }
     else{
        printf NHGR "%s %f\n",$h,log($diff/($totcnt+$diff))/$log10;
      }
     

     #reset smoothing statistics
     $code=-1;@cnt=(); @dict=(); $totcnt=0;$diff=0;$oldwrd="";$N1=0;$N2=0;$locfreq=0;
  
     #read next history
     chop($h=<HGR>);  @h=split(/ /,$h); $hpr=shift @h;
  
 }until ($ng eq ""); #n-grams are over

 close(HGR); close(INP);close(GR);close(NGR);
 rename("${sublm}.".($n-1)."ngr.gz","${sublm}.".($n-1)."gr.gz");
}   

