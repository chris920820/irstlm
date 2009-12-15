// $Id$

/******************************************************************************
IrstLM: IRST Language Model Toolkit
Copyright (C) 2006 Marcello Federico, ITC-irst Trento, Italy

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

******************************************************************************/


#ifndef MF_LMTABLE_H
#define MF_LMTABLE_H

#ifndef WIN32
#include <sys/types.h>
#include <sys/mman.h>
#endif

#include <math.h>
#include <cstdlib>

#include "util.h"
#include "ngramcache.h"
#include "dictionary.h"
#include "n_gram.h"

#define MAX(a,b) (((a)>(b))?(a):(b))

//#undef TRACE_CACHE

#define LMTMAXLEV  20
#define MAX_LINE  1024

#ifndef  LMTCODESIZE
#define  LMTCODESIZE  (int)3
#endif

#define SHORTSIZE (int)2
#define PTRSIZE   (int)sizeof(char *)
#define INTSIZE   (int)4
#define CHARSIZE  (int)1

#define PROBSIZE  (int)4 //use float  
#define QPROBSIZE (int)1 
#define BOUNDSIZE (int)4

#define UNIGRAM_RESOLUTION 10000000.0

typedef enum {INTERNAL,QINTERNAL,LEAF,QLEAF} LMT_TYPE;
typedef enum {BINARY,TEXT,NONE} OUTFILE_TYPE;
typedef char* node;

typedef enum {LMT_FIND,    //!< search: find an entry
  LMT_ENTER,   //!< search: enter an entry 
  LMT_INIT,    //!< scan: start scan
  LMT_CONT     //!< scan: continue scan
} LMT_ACTION;

typedef unsigned int  table_pos_t;

//CHECK this part from HERE
//if table_pos_t is "long"
#define table_pos_t_ALLONE 0xff

//if table_pos_t is "long long"
//#define table_pos_t_ALLONE 0xffLL

//if table_pos_t is "unsigned long long"
//#define table_pos_t_ALLONE 0xffULL

//CHECK this part to HERE

#define BOUND_EMPTY1 (numeric_limits<table_pos_t>::max() - 2)
#define BOUND_EMPTY2 (numeric_limits<table_pos_t>::max() - 1)

//#define BOUND_EMPTY BOUND_EMPTY2

//#define BOUND_EMPTY1 (-1)
//#define BOUND_EMPTY2 (-2)

class lmtable{
  
 protected:
  char*       table[LMTMAXLEV+1];  //storage of all levels
  LMT_TYPE    tbltype[LMTMAXLEV+1];  //table type for each levels
  table_pos_t       cursize[LMTMAXLEV+1];  //current size of levels
  table_pos_t       maxsize[LMTMAXLEV+1];  //max size of levels
  table_pos_t*     startpos[LMTMAXLEV+1];  //support vector to store start positions
	
  int               maxlev; //max level of table
  char           info[100]; //information put in the header
  
  //statistics 
  int    totget[LMTMAXLEV+1];
  int    totbsearch[LMTMAXLEV+1];
  
  //probability quantization
  bool      isQtable;
  
  //Incomplete LM table from distributed training
  bool      isItable;

  //Table might contain pruned n-grams
  bool      isPruned; 
   
  int       NumCenters[LMTMAXLEV+1];
  float*    Pcenters[LMTMAXLEV+1];
  float*    Bcenters[LMTMAXLEV+1];
  
  double  logOOVpenalty; //penalty for OOV words (default 0)
  int     dictionary_upperbound; //set by user
  int     backoff_state; 
  
  //improve access speed
  ngramcache* lmtcache[LMTMAXLEV+1];
  ngramcache* probcache;
  ngramcache* statecache; 
  ngramcache* statesizecache;
  int max_cache_lev;

  //memory map on disk
  int memmap;  //level from which n-grams are accessed via mmap
  int diskid;
  off_t tableOffs[LMTMAXLEV+1];
  off_t tableGaps[LMTMAXLEV+1];

  // is this LM queried for knowing the matching order or (standard
  // case) for score?
  bool      orderQuery;
  
public:
    
#ifdef TRACE_CACHE
    std::fstream* cacheout;
  int sentence_id;
#endif
  
  dictionary     *dict; // dictionary (words - macro tags)
  
  lmtable();
  
  virtual ~lmtable(){
    for (int i=2;i<=LMTMAXLEV;i++)
    if (lmtcache[i]){
      //std::cerr << i <<"-gram cache: "; lmtcache[i]->stat();
      delete lmtcache[i];
    }

    if (probcache){
      //std::cerr << "Prob Cache: "; probcache->stat();
      delete probcache;
#if TRACE_CACHE
      cacheout->close();
      delete cacheout;
#endif

    }
    if (statecache){
      //std::cerr << "State Cache: "; statecache->stat();
      delete statecache;
	  delete statesizecache;
    }


    for (int l=1;l<=maxlev;l++){
      if (table[l]){
          if (memmap > 0 && l >= memmap)
            Munmap(table[l]-tableGaps[l],cursize[l]*nodesize(tbltype[l])+tableGaps[l],0);
        else
          delete [] table[l];
      }
      if (isQtable){
        if (Pcenters[l]) delete [] Pcenters[l];
                                if (l<maxlev)
          if (Bcenters[l]) delete [] Bcenters[l];
      }
    }
  };


  table_pos_t wdprune(float *thr, int aflag=0);
  table_pos_t wdprune(float *thr, int aflag, ngram ng, int ilev, int elev, table_pos_t ipos, table_pos_t epos,
                double lk=0, double bo=0, double *ts=0, double *tbs=0);
  double lprobx(ngram ong, double *lkp=0, double *bop=0, int *bol=0);

  table_pos_t ngcnt(table_pos_t *cnt);
  table_pos_t ngcnt(table_pos_t *cnt, ngram ng, int l, table_pos_t ipos, table_pos_t epos);
  int pscale(int lev, table_pos_t ipos, table_pos_t epos, double s);
    
  void init_probcache();
  void init_statecache();
  void init_lmtcaches(int uptolev);
  
  void check_cache_levels();
  void reset_caches();
 
  void reset_mmap();
       
  bool is_probcache_active(){return probcache!=NULL;}
  bool is_statecache_active(){return statecache!=NULL;}
  bool are_lmtcaches_active(){return lmtcache[2]!=NULL;}  
  
  void configure(int n,bool quantized);
    
 //set penalty for OOV words  
  double getlogOOVpenalty() const { return logOOVpenalty; }
  
  double setlogOOVpenalty(int dub){ 
    assert(dub > dict->size());
    return logOOVpenalty=log((double)(dub - dict->size()))/log(10.0);
  }
  
  double setlogOOVpenalty2(double oovp){ 
    return logOOVpenalty=oovp;
  }
  
  int maxlevel() const {return maxlev;};
  bool isQuantized() const {return isQtable;}
  
  
  void savetxt(const char *filename);
  void savebin(const char *filename);
  void dumplm(std::fstream& out,ngram ng, int ilev, int elev, table_pos_t ipos,table_pos_t epos);
  
  void load(std::istream& inp,const char* filename=NULL,const char* outfilename=NULL,int mmap=0,OUTFILE_TYPE outtype=NONE);
  void loadtxt(std::istream& inp,const char* header,const char* outfilename,int mmap);
  void loadtxt(std::istream& inp,const char* header);
  void loadtxtmmap(std::istream& inp,const char* header,const char* outfilename);
  void loadbin(std::istream& inp,const char* header,const char* filename=NULL,int mmap=0);
  
  void loadbinheader(std::istream& inp, const char* header);
  void loadbincodebook(std::istream& inp,int l);
  
  lmtable* cpsublm(dictionary* subdict,bool keepunigr=true);
  
  void filter(const char* lmfile){};
  void filter2(const char* lmfile,int buffMb=512){
    std::cerr << "function is no more available\n";
    exit(0);
  };
  
  void loadcenters(std::istream& inp,int Order);
	
  virtual double lprob(ngram ng, double* bow=NULL,int* bol=NULL,int internalcall=0);
  //virtual double lprob(ngram ng);
  virtual double clprob(ngram ng); 
  
  
  void *search(int lev,table_pos_t offs,table_pos_t n,int sz,int *w,
               LMT_ACTION action,char **found=(char **)NULL);
  
//  int mybsearch(char *ar, table_pos_t n, int size, unsigned char *key, table_pos_t *idx);   
  int mybsearch(char *ar, table_pos_t n, int size, char *key, table_pos_t *idx);   
  
  int add(ngram& ng,int prob,int bow);
  void checkbounds(int level);

  int get(ngram& ng){return get(ng,ng.size,ng.size);}
  int get(ngram& ng,int n,int lev);
  
  int succscan(ngram& h,ngram& ng,LMT_ACTION action,int lev);
  
  virtual const char *maxsuffptr(ngram ong, unsigned int* size=NULL);
  virtual const char *cmaxsuffptr(ngram ong, unsigned int* size=NULL);
  
  inline void putmem(char* ptr,int value,int offs,int size){
    assert(ptr!=NULL);
    for (int i=0;i<size;i++)
      ptr[offs+i]=(value >> (8 * i)) & 0xff;
  };
  
  inline void getmem(char* ptr,int* value,int offs,int size){
    assert(ptr!=NULL);
    *value=ptr[offs] & 0xff;
    for (int i=1;i<size;i++)
      *value= *value | ( ( ptr[offs+i] & 0xff ) << (8 *i));
  };
  
  inline void putmem(char* ptr,table_pos_t value,int offs,int size){
    assert(ptr!=NULL);
    for (int i=0;i<size;i++) 
      ptr[offs+i]=(value >> (8 * i)) & table_pos_t_ALLONE;
  };
  
  inline void getmem(char* ptr,table_pos_t* value,int offs,int size){
    assert(ptr!=NULL);
    *value=ptr[offs] & table_pos_t_ALLONE;
    for (int i=1;i<size;i++) 
      *value= *value | ( ( ptr[offs+i] & table_pos_t_ALLONE ) << (8 *i));
  };


  
  int nodesize(LMT_TYPE ndt){
    switch (ndt){
      case INTERNAL:
        return LMTCODESIZE + PROBSIZE + PROBSIZE + BOUNDSIZE;
      case QINTERNAL:
        return LMTCODESIZE + QPROBSIZE + QPROBSIZE + BOUNDSIZE;
      case QLEAF:
        return LMTCODESIZE + QPROBSIZE;      
      case LEAF:
        return LMTCODESIZE + PROBSIZE;      
      default:
        assert(0);
        return 0;
    }
  }  
  
  inline int word(node nd,int value=-1)
  {
    int offset=0;
    
    if (value==-1)
      getmem(nd,&value,offset,LMTCODESIZE);
    else
      putmem(nd,value,offset,LMTCODESIZE);
    
    return value;
  };
  
  inline int prob(node nd,LMT_TYPE ndt, int value=-1)
  {
    int offs=LMTCODESIZE;
    int size=(ndt==QINTERNAL || ndt==QLEAF?QPROBSIZE:PROBSIZE);
    
    if (value==-1)
      getmem(nd,&value,offs,size);
    else
      putmem(nd,value,offs,size);
    
    return value;
  };
  
  
  inline int bow(node nd,LMT_TYPE ndt, int value=-1)
  {
    assert(ndt==INTERNAL || ndt==QINTERNAL);
    int size=(ndt==QINTERNAL?QPROBSIZE:PROBSIZE);
    int offs=LMTCODESIZE+size;
    
    if (value==-1)
      getmem(nd,&value,offs,size);
    else
      putmem(nd,value,offs,size);
    
    return value;
  };
  
  inline table_pos_t bound(node nd,LMT_TYPE ndt, table_pos_t value=BOUND_EMPTY2)
  {
    assert(ndt==INTERNAL || ndt==QINTERNAL);
    int offs=LMTCODESIZE+2*(ndt==QINTERNAL?QPROBSIZE:PROBSIZE);
    
    if (value==BOUND_EMPTY2)
      getmem(nd,&value,offs,BOUNDSIZE);
    else
      putmem(nd,value,offs,BOUNDSIZE);
    
    return value;
  };
  
  void stat(int lev=0);


  void printTable(int level);

  virtual inline dictionary* getDict() const {
    return dict;
  };

  inline void setOrderQuery(bool v) 
    {
      orderQuery = v;
    }
  inline bool isOrderQuery() const
    {
      return orderQuery;
    }
};


#endif

