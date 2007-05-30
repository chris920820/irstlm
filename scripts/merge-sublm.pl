#! /usr/bin/perl

#*****************************************************************************
# IrstLM: IRST Language Model Toolkit
# Copyright (C) 2007 Marcello Federico, ITC-irst Trento, Italy

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

#******************************************************************************
#merge prefix LMs into one single file

use strict;
use Getopt::Long "GetOptions";

my ($help,$lm,$size,$sublm)=();
$help=1 unless
&GetOptions('size=i' => \$size,
            'lm=s' => \$lm,
            'sublm=s' => \$sublm,
            'help' => \$help,);


if ($help || !$size || !$lm || !$sublm){
  print "merge-sublm.pl <options>\n",
  "--size <int>        maximum n-gram size for the language model\n",
  "--sublm <string>    path identifying  all prefix sub LMs \n",
  "--lm <string>       name of final LM file (will be gzipped)\n",
  "--help              (optional) print these instructions\n";    
  exit(1);
}


my $gzip="/usr/bin/gzip";   
my $gunzip="/usr/bin/gunzip";

warn "merge-sublm.pl --size $size --sublm $sublm --lm $lm\n";

warn "Compute total sizes of n-grams\n";
my @size=();         #number of n-grams for each level
my $tot1gr=0;         #total frequency of 1-grams
my $unk=0;            #frequency of <unk>
my $pr;               #probability of 1-grams
my (@files,$files);  #sublm files for a given n-gram size  

for (my $n=1;$n<=$size;$n++){

  @files=map { glob($_) } "${sublm}*.${n}gr*";
  $files=join(" ",@files);
  $files || die "cannot find sublm files\n";
  
  open(INP,"$gunzip -c $files|") || die "cannot open $files\n";
  while(<INP>){
    $size[$n]++;
    if ($n==1){
      chop;split(" ",$_);
      #cut down counts for sentence initial
      $_[0]=1 if $_[1]=~/<s>/;
      $unk=$_[0] if $_[1]=~/<unk>/;
      $tot1gr+=$_[0];
    }
  }
  if ($n==1 && $unk==0){
    #implicitely add <unk> word to counters
    $tot1gr+=$size[$n]; #equivalent to WB smoothing
    $size[$n]++; 
  }
  close(INP);
}



warn "Merge all sub LMs\n";

$lm.=".gz" if $lm!~/.gz$/;
open(LM,"|$gzip -c > $lm") || die "Cannot open $lm\n";

warn "Write LM Header\n";
printf LM "iARPA\n";

printf LM "\n\\data\\\n";
for (my $n=1;$n<=$size;$n++){
    printf LM "ngram $n= $size[$n]\n";
}
printf LM "\n\n";
warn "Writing LM Tables\n";
for (my $n=1;$n<=$size;$n++){
  
  warn "Level $n\n";
  
  @files=map { glob($_) } "${sublm}*.${n}gr*";
  $files=join(" ",@files);
  open(INP,"$gunzip -c $files|") || die "cannot open $files\n";
  warn "input from: $files\n";
  printf LM "\\$n-grams:\n";
  while(<INP>){   
    
    if ($n==1){         
      split(" ",$_);
      #cut down counts for sentence initial
      $_[0]=1 if $_[1]=~/<s>/;
      #apply witten-bell smoothing on 1-grams
      $pr=(log($_[0]+1)-log($tot1gr+$size[1]))/log(10.0);shift @_;
      printf LM "%f %s\n",$pr,join(" ",@_);
    }
    else{
      printf LM "%s",$_;
    }
  }
  if ($n==1 && $unk==0){
    warn "eventually add <unk>\n";
    $pr=(log($size[1]-1+1)-log($tot1gr+$size[1]))/log(10.0);
    printf LM "%f <unk>\n",$pr;
  }
  close(INP);
}

printf LM "\\end\\\n";
close(LM);


