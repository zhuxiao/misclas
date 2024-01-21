#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <htslib/sam.h>
#include <pthread.h>
#include <htslib/thread_pool.h>

#include "Base.h"
#include "structures.h"

using namespace std;

vector<string> split(const  string& s, const string& delim);
bool isExistStr(string &str, vector<string> &vec);
int copySingleFile(string &infilename, ofstream &outfile);
char getBase(string &seq, size_t pos, size_t orient);
void reverseSeq(string &seq);
void reverseComplement(string &seq);
void upperSeq(string &seq);
size_t getCtgCount(string &contigfilename);
reg_t* findVarvecItem(int32_t startPos, int32_t endPos, vector<reg_t*> &varVec);
vector<reg_t*> findVarvecItemAll(int32_t startPos, int32_t endPos, vector<reg_t*> &varVec);
reg_t* findVarvecItemExtSize(int32_t startRefPos, int32_t endRefPos, vector<reg_t*> &varVec, int32_t leftExtSize, int32_t rightExtSize);
vector<reg_t*> findVarvecItemAllExtSize(int32_t startRefPos, int32_t endRefPos, vector<reg_t*> &varVec, int32_t leftExtSize, int32_t rightExtSize);
int32_t getVectorIdx(reg_t *reg, vector<reg_t*> &varVec);
reg_t* getOverlappedReg(reg_t *reg, vector<reg_t*> &varVec);
int32_t getOverlappedRegIdx(reg_t *reg, vector<reg_t*> &varVec);
bool isOverlappedReg(reg_t* reg1, reg_t* reg2);
bool isOverlappedPos(size_t startPos1, size_t endPos1, size_t startPos2, size_t endPos2);
int32_t getOverlapSize(size_t startPos1, size_t endPos1, size_t startPos2, size_t endPos2);
bool isAdjacent(size_t startPos1, size_t endPos1, size_t startPos2, size_t endPos2, int32_t dist_thres);
bam_hdr_t* loadSamHeader(string &inBamFile);
bool isInReg(int32_t pos, vector<reg_t*> &vec);
int32_t computeDisagreeNum(Base *baseArray, int32_t arr_size);
void mergeOverlappedReg(vector<reg_t*> &regVector);
void updateReg(reg_t* reg1, reg_t* reg2);
void mergeAdjacentReg(vector<reg_t*> &regVec, size_t dist_thres);
void printRegVec(vector<reg_t*> &regVec, string header);
string preprocessPipeChar(string &cmd_str);
bool isFileExist(string &filename);
void removeRedundantItems(vector<reg_t*> &reg_vec);
int32_t getLineCount(string &filename);
bool isBaseMatch(char ctgBase, char refBase);
bool isRegValid(reg_t *reg);
void exchangeRegLoc(reg_t *reg);
void blatAln(string &alnfilename, string &contigfilename, string &refseqfilename);
void cleanPrevAssembledTmpDir(const string &assem_dir_str, const string &dir_prefix);
string getCallFileHeaderBed();
string getCallFileHeaderBedpe();
assembleWork_opt* allocateAssemWorkOpt(string &chrname, string &readsfilename, string &contigfilename, string &refseqfilename, string &tmpdir, vector<reg_t*> &varVec, bool clip_reg_flag, bool limit_reg_process_flag, vector<simpleReg_t*> &limit_reg_vec);
void releaseAssemWorkOpt(assembleWork_opt *assem_work_opt);
void destroyAssembleWorkOptVec(vector<assembleWork_opt*> &assem_work_vec);
void *doit_canu(void *arg);
int test_canu(int n, vector<string> &cmd_vec);
void* processSingleAssembleWork(void *arg);
void performLocalAssembly(string &readsfilename, string &contigfilename, string &refseqfilename, string &tmpdir, size_t num_threads_per_assem_work, vector<reg_t*> &varVec, string &chrname, string &inBamFile, faidx_t *fai, ofstream &assembly_info_file, double expected_cov_assemble, bool delete_reads_flag, bool limit_reg_process_flag, vector<simpleReg_t*> &limit_reg_vec);
void outputAssemWorkOptToFile_debug(vector<assembleWork_opt*> &assem_work_opt_vec);
string getOldOutDirname(string &filename, string &sub_work_dir);
string getUpdatedItemFilename(string &filename, string &out_dir, string &old_out_dir);
string deleteTailPathChar(string &dirname);
// get overlapped simple regions
vector<simpleReg_t*> getOverlappedSimpleRegsExt(string &chrname, int64_t begPos, int64_t endPos, vector<simpleReg_t*> &limit_reg_vec, int32_t ext_size);
vector<simpleReg_t*> getOverlappedSimpleRegs(string &chrname, int64_t begPos, int64_t endPos, vector<simpleReg_t*> &limit_reg_vec);
vector<simpleReg_t*> getFullyContainedSimpleRegs(string &chrname, int64_t begPos, int64_t endPos, vector<simpleReg_t*> &limit_reg_vec);
bool isFullyContainedReg(string &chrname1, int64_t begPos1, int64_t endPos1, string &chrname2, int64_t startPos2, int64_t endPos2);
vector<simpleReg_t*> getPosContainedSimpleRegs(string &chrname, int64_t begPos, int64_t endPos, vector<simpleReg_t*> &limit_reg_vec);
simpleReg_t* allocateSimpleReg(string &simple_reg_str);
void destroyLimitRegVector(vector<simpleReg_t*> &limit_reg_vec);
void printLimitRegs(vector<simpleReg_t*> &limit_reg_vec, string &description);
void getRegByFilename(simpleReg_t *reg, string &filename, string &pattern_str);
vector<simpleReg_t*> extractSimpleRegsByStr(string &regs_str);
string getLimitRegStr(vector<simpleReg_t*> &limit_reg_vec);
void *processSingleMisWork(void *arg);

class Time{
	private:
		time_t timestamp, start, end, start_all, end_all;
		struct tm *time_tm;
		struct timeval time_val;

	public:
		Time();
		virtual ~Time();
		string getTime();
		void printTime();
		void setStartTime();
		void printSubCmdElapsedTime();
		void printOverallElapsedTime();
};

#endif /* SRC_UTIL_H_ */
