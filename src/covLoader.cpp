#include "covLoader.h"

covLoader::covLoader(string &chrname, size_t startPos, size_t endPos, faidx_t *fai) {
	this->chrname = chrname;
	this->startPos = startPos;
	this->endPos = endPos;
	this->fai = fai;
	min_ins_size_filt =	min_del_size_filt = 0;
	bam_type = BAM_INVALID;
}

covLoader::covLoader(string &chrname, size_t startPos, size_t endPos, faidx_t *fai, size_t min_ins_size_filt, size_t min_del_size_filt) {
	this->chrname = chrname;
	this->startPos = startPos;
	this->endPos = endPos;
	this->fai = fai;
	this->min_ins_size_filt = min_ins_size_filt;
	this->min_del_size_filt = min_del_size_filt;
	bam_type = BAM_INVALID;
}

covLoader::~covLoader() {
}

// initialize the base array of the block
Base *covLoader::initBaseArray(){
	Base *baseArray = new Base[endPos-startPos+1]();
	if(!baseArray){
		cerr << __func__ << ": cannot allocate memory" << endl;
		exit(1);
	}
	// assign the ref base index in base coverage
	assignRefBase(baseArray, fai);
	return baseArray;
}

void covLoader::freeBaseArray(Base *baseArray){
	if(baseArray){
		delete[] baseArray;
		baseArray = NULL;
	}
}

int covLoader::assignRefBase(Base *baseArray, faidx_t *fai){
	size_t pos;
	// get the region sequence
	string reg = chrname + ":" + to_string(startPos) + "-" + to_string(endPos);

	RefSeqLoader refseq_loader(reg, fai);
	refseq_loader.getRefSeq();

	pos = startPos;
	for(int i=0; i<refseq_loader.refseq_len; i++){
		switch(refseq_loader.refseq[i]){
			case 'A':
			case 'a': baseArray[i].coverage.idx_RefBase = 0; break;
			case 'C':
			case 'c': baseArray[i].coverage.idx_RefBase = 1; break;
			case 'G':
			case 'g': baseArray[i].coverage.idx_RefBase = 2; break;
			case 'T':
			case 't': baseArray[i].coverage.idx_RefBase = 3; break;
			case 'N':
			case 'n': baseArray[i].coverage.idx_RefBase = 4; break;
			case 'M':
			case 'm':
			case 'R':
			case 'r':
			case 'S':
			case 's':
			case 'V':
			case 'v':
			case 'W':
			case 'w':
			case 'Y':
			case 'y':
			case 'H':
			case 'h':
			case 'K':
			case 'k':
			case 'D':
			case 'd':
			case 'B':
			case 'b': baseArray[i].coverage.idx_RefBase = 5; break;
			default: cerr << __func__ << ": unknown base: " << refseq_loader.refseq[i] << " at location: " << chrname << ":" << pos << endl; exit(1);
		}
		baseArray[i].coverage.refBase = refseq_loader.refseq[i];
		pos ++;
	}
	return 0;
}

// assign base coverage
void covLoader::generateBaseCoverage(Base *baseArr, vector<bam1_t*> alnDataVector){
	vector<struct alnSeg*> alnSegs;
	vector<bam1_t*>::iterator aln;
	//string qname;
	if(alnDataVector.size()>0){
		bam_type = getBamType(*alnDataVector.begin());
		if(bam_type == BAM_INVALID){
			cerr << __func__ << ": unknown bam_type, error!" << endl;
			exit(1);
		}

		for(aln=alnDataVector.begin(); aln!=alnDataVector.end(); aln++)
			if(!((*aln)->core.flag & BAM_FUNMAP)){ // aligned
				//qname = bam_get_qname(*aln);
				//if(qname.compare("cc7f944e_142691_4447")==0){
				//	cout << qname << endl;
				//}
				switch(bam_type){
					case BAM_CIGAR_NO_DIFF_MD:
						alnSegs = generateAlnSegs(*aln);
						break;
					case BAM_CIGAR_DIFF_NO_MD:
						alnSegs = generateAlnSegsDiff_no_MD(*aln);
						break;
				}// generate align segments

				updateBaseInfo(baseArr, alnSegs); // update base information
				destroyAlnSegs(alnSegs);
			}

		updateBaseCovInfo(baseArr);

		// compute number of deletions
		computeDelNumFromDelVec(baseArr);
	}
}

// generate the alignment segments
vector<struct alnSeg*> covLoader::generateAlnSegs(bam1_t* b){
	vector<struct alnSeg*> alnSegs;
	vector<struct MD_seg*> segs_MD;
	struct MD_seg* seg_MD;

	uint32_t *c, op, i = 0, j = 0, startRpos, startQpos;
	int32_t k, tmp_cigar_len, tmp_MD_len, common_len;  // common_len: used only for match flag
	string ins_str;

	startRpos = b->core.pos + 1;  // 1-based
	startQpos = 1;

	c = bam_get_cigar(b);  // CIGAR
	op = bam_cigar_op(c[i]);
	tmp_cigar_len = bam_cigar_oplen(c[i]);

	segs_MD = extractMDSegs(b);
	seg_MD = segs_MD[j];
	tmp_MD_len = seg_MD->seglen;
	while(i<b->core.n_cigar or j<segs_MD.size()){
		common_len = -1;
		switch(op){
			case BAM_CMATCH:
				if(tmp_cigar_len>=tmp_MD_len) // tmp_cigar_len >= seg_MD.len
					common_len = tmp_MD_len;
				else // tmp_cigar_len < seg_MD.len
					common_len = tmp_cigar_len;

				// add alnSeg item
				if(common_len==1 and seg_MD->opflag==MD_MISMATCH){
					// change the mismatched reference base to query base
					seg_MD->seg = "=ACMGRSVTWYHKDBN"[bam_seqi(bam_get_seq(b), startQpos-1)];
					alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, common_len, MD_MISMATCH, seg_MD->seg));
				}else
					alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, common_len, BAM_CMATCH, ""));

				startRpos += common_len;
				startQpos += common_len;

				tmp_cigar_len -= common_len;
				tmp_MD_len -= common_len;

				if(tmp_cigar_len==0 and ++i<b->core.n_cigar){
					op = bam_cigar_op(c[i]);
					tmp_cigar_len = bam_cigar_oplen(c[i]);
				}
				if(tmp_MD_len==0 and ++j<segs_MD.size()){
					seg_MD = segs_MD[j];
					tmp_MD_len = seg_MD->seglen;
				}
				break;
			case BAM_CINS:
				// add alnSeg item
				ins_str = "";
				for(k=0; k<tmp_cigar_len; k++) ins_str += "=ACMGRSVTWYHKDBN"[bam_seqi(bam_get_seq(b), startQpos+k-1)];
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, BAM_CINS, ins_str));
				startQpos += tmp_cigar_len;
				op = bam_cigar_op(c[++i]);
				tmp_cigar_len = bam_cigar_oplen(c[i]);

				break;
			case BAM_CDEL:
				// add alnSeg item
				if(tmp_cigar_len==tmp_MD_len){
					if(seg_MD->opflag==BAM_CDEL){
						alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, BAM_CDEL, seg_MD->seg));
						startRpos += tmp_cigar_len;
						op = bam_cigar_op(c[++i]);
						tmp_cigar_len = bam_cigar_oplen(c[i]);
						seg_MD = segs_MD[++j];
						tmp_MD_len = seg_MD->seglen;
					}else{
						cerr << __func__ << ", line=" << __LINE__ << ": error" << endl;
						exit(1);
					}
				}else{
					cerr << __func__ << ", line=" << __LINE__ << ": error" << endl;
					exit(1);
				}
				break;
			case BAM_CSOFT_CLIP:
			case BAM_CHARD_CLIP:
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, op, to_string(tmp_cigar_len)));
				if(op==BAM_CSOFT_CLIP) startQpos += tmp_cigar_len;
				if(++i<b->core.n_cigar){
					op = bam_cigar_op(c[i]);
					tmp_cigar_len = bam_cigar_oplen(c[i]);
				}
				break;
			case BAM_CREF_SKIP:  // unexpected events
			case BAM_CPAD:
			case BAM_CEQUAL:
			case BAM_CDIFF:
			case BAM_CBACK:
			default:
				cerr << __func__ << ", line=" << __LINE__ << ": invalid opflag " << op << endl;
				exit(1);
		}
	}
	destroyMDSeg(segs_MD);
	return alnSegs;
}

vector<struct alnSeg*>covLoader::generateAlnSegsDiff_no_MD(bam1_t* b){
	vector<struct alnSeg*> alnSegs;
//	vector<struct MD_seg*> segs_MD;
//	struct MD_seg* seg_MD;

	uint32_t *c, op, i = 0,startRpos, startQpos;
	int32_t k, tmp_cigar_len, common_len;  // common_len: used only for match flag
	string ins_str,diff,del;

	startRpos = b->core.pos + 1;  // 1-based
	startQpos = 1;

	c = bam_get_cigar(b);  // CIGAR
	op = bam_cigar_op(c[i]);
	tmp_cigar_len = bam_cigar_oplen(c[i]);

//	segs_MD = extractMDSegs(b);
//	seg_MD = segs_MD[j];
//	tmp_MD_len = seg_MD->seglen;
	while(i<b->core.n_cigar){
		common_len = -1;
		switch(op){
			case BAM_CMATCH:
				i++;
				break;
			case BAM_CINS:
				// add alnSeg item
				ins_str = "";
				for(k=0; k<tmp_cigar_len; k++) ins_str += "=ACMGRSVTWYHKDBN"[bam_seqi(bam_get_seq(b), startQpos+k-1)];
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, BAM_CINS, ins_str));
				startQpos += tmp_cigar_len;
				op = bam_cigar_op(c[++i]);
				tmp_cigar_len = bam_cigar_oplen(c[i]);

				break;
			case BAM_CDEL:
				// add alnSeg item
				if(op==BAM_CDEL){
					del = "=ACMGRSVTWYHKDBN"[bam_seqi(bam_get_seq(b), startQpos-1)];
					alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, BAM_CDEL, del));
					startQpos += tmp_cigar_len;
					op = bam_cigar_op(c[++i]);
					tmp_cigar_len = bam_cigar_oplen(c[i]);
				}else{
					cerr << __func__ << ", line=" << __LINE__ << ": error" << endl;
					exit(1);
				}
				break;
			case BAM_CSOFT_CLIP:
				i++;
				break;
			case BAM_CHARD_CLIP:
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, tmp_cigar_len, op, to_string(tmp_cigar_len)));
				if(op==BAM_CSOFT_CLIP) startQpos += tmp_cigar_len;
				if(++i<b->core.n_cigar){
					op = bam_cigar_op(c[i]);
					tmp_cigar_len = bam_cigar_oplen(c[i]);
				}
				break;
			case BAM_CREF_SKIP:  // unexpected events
				i++;
				break;
			case BAM_CPAD:
				i++;
				break;
			case BAM_CEQUAL:
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, common_len, BAM_CMATCH, ""));
				startQpos += tmp_cigar_len;
				op = bam_cigar_op(c[++i]);
				tmp_cigar_len = bam_cigar_oplen(c[i]);
				break;
			case BAM_CDIFF:

				diff = "=ACMGRSVTWYHKDBN"[bam_seqi(bam_get_seq(b), startQpos-1)];
				alnSegs.push_back(allocateAlnSeg(startRpos, startQpos, common_len, MD_MISMATCH, diff));
				startQpos += tmp_cigar_len;
				op = bam_cigar_op(c[++i]);
				tmp_cigar_len = bam_cigar_oplen(c[i]);
				break;
			case BAM_CBACK:
				i++;
				break;
			default:
				cerr << __func__ << ", line=" << __LINE__ << ": invalid opflag " << op << endl;
				exit(1);
		}
	}
	return alnSegs;
}


struct alnSeg* covLoader::allocateAlnSeg(size_t startRpos, size_t startQpos, size_t seglen, size_t opflag, string seg_MD){
	struct alnSeg* seg = new struct alnSeg;
	if(!seg){
		cerr << __func__ << ": cannot allocate memory" << endl;
		exit(1);
	}
	seg->startRpos = startRpos;
	seg->startQpos = startQpos;
	seg->seglen = seglen;
	seg->opflag = opflag;
	seg->seg_MD = seg_MD;
	return seg;
}

// release the alignment segments
void covLoader::destroyAlnSegs(vector<struct alnSeg*> alnSegs){
	vector<struct alnSeg*>::iterator seg;
	for(seg=alnSegs.begin(); seg!=alnSegs.end(); seg++) delete *seg;
	vector<struct alnSeg*>().swap(alnSegs);
}

//estimate the kind of bamfile
int covLoader::getBamType(bam1_t *b){
	uint32_t *c, op,i;
	uint8_t *flag;
	flag = bam_aux_get(b,"MD");
	if ( flag ){
		for(i=0; i<b->core.n_cigar; i++){
			c = bam_get_cigar(b);  // CIGAR
			op = bam_cigar_op(c[i]);
			if(op == BAM_CMATCH){
				// cout<<"there are MD flag and MATCH in the bamfile"<<endl;
				return BAM_CIGAR_NO_DIFF_MD;
			}
		}
		for(i=0; i<b->core.n_cigar; i++){
			c = bam_get_cigar(b);  // CIGAR
			op = bam_cigar_op(c[i]);
			if(op == BAM_CEQUAL || op == BAM_CDIFF){
				// cout<<"there are MD flag and DIFF in the bamfile"<<endl;
				return BAM_CIGAR_DIFF_MD;
			}
		}
	}else {
		for(i=0; i<b->core.n_cigar; i++){
			c = bam_get_cigar(b);  // CIGAR
			op = bam_cigar_op(c[i]);
			if(op == BAM_CMATCH){
				// cout<<"there is not MD flag but having MATCH in the bamfile"<<endl;
				return BAM_CIGAR_NO_DIFF_NO_MD;
			}
		}
		for(i=0; i<b->core.n_cigar; i++){
			c = bam_get_cigar(b);  // CIGAR
			op = bam_cigar_op(c[i]);
			if(op == BAM_CEQUAL || op == BAM_CDIFF){
				// cout<<"there is not MD flag but having DIFFH in the bamfile"<<endl;
				return BAM_CIGAR_DIFF_NO_MD;
			}
		}
	}

	cout<<"invalid"<<endl;
	return BAM_INVALID;
}

// get the MD segs
vector<struct MD_seg*> covLoader::extractMDSegs(bam1_t* b){
	vector<struct MD_seg*> segs_MD;
	char *p;
	string seg;

	p = bam_aux2Z(bam_aux_get(b, "MD"));  // MD
	while(*p){
		seg = "";
		if(*p=='^'){  // deletion
			p++;  // omit the '^'
			while((*p>='A' and *p<='Z') or (*p>='a' and *p<='z')) seg += *p++;
			// allocate the MD seg node, and add it to vector
			segs_MD.push_back(allocateMDSeg(seg, BAM_CDEL));
		}else if(*p>='0' and *p<='9'){  // match
			seg += *p++;
			while(*p>='0' and *p<='9') seg += *p++;
			// allocate the MD seg node, and add it to vector
			if(seg!="0") segs_MD.push_back(allocateMDSeg(seg, BAM_CMATCH));
		}else if((*p>='A' and *p<='Z') or (*p>='a' and *p<='z')){ // mismatch
			seg += *p++;
			if((*p>='A' and *p<='Z') or (*p>='a' and *p<='z')){
				cerr << __func__ << ": invalid seg" << endl;
				exit(1);
			}

			// allocate the MD seg node, and add it to vector
			segs_MD.push_back(allocateMDSeg(seg, MD_MISMATCH));
		}else{
			cerr << __func__ << ": invalid seg" << endl;
			exit(1);
		}
	}

	return segs_MD;
}

// allocate MD seg node
struct MD_seg* covLoader::allocateMDSeg(string& seg, size_t opflag){
	struct MD_seg* seg_MD = new struct MD_seg;
	if(!seg_MD){
		cerr << __func__ << ": cannot allocate memory" << endl;
		exit(1);
	}
	seg_MD->seg = seg;
	seg_MD->opflag = opflag;
	switch(opflag){
		case BAM_CMATCH: seg_MD->seglen = stoi(seg_MD->seg); break;
		case BAM_CDEL:
		case MD_MISMATCH: seg_MD->seglen = seg_MD->seg.size(); break;
		default:
			cerr << __func__ << ": invalid opflag:" << opflag << endl; exit(1);
	}
	return seg_MD;
}

// destroy MD seg vector
void covLoader::destroyMDSeg(vector<struct MD_seg*> segs_MD){
	vector<struct MD_seg*>::iterator seg;
	for(seg=segs_MD.begin(); seg!=segs_MD.end(); seg++) delete *seg;
	vector<struct MD_seg*>().swap(segs_MD);
}

// update the block base array information according to read alignments
int covLoader::updateBaseInfo(Base *baseArr, vector<struct alnSeg*> alnSegs){
	baseCoverage_t *cover;
	size_t pos, tmp_endPos, misbase;
	int32_t idx, endflag;
	vector<struct alnSeg*>::iterator seg;

	for(seg=alnSegs.begin(); seg!=alnSegs.end(); seg++){
		switch((*seg)->opflag){
			case BAM_CMATCH:
					tmp_endPos = (*seg)->startRpos + (*seg)->seglen - 1;
					for(pos=(*seg)->startRpos; pos<=tmp_endPos; pos++)
						if(pos>=startPos and pos<=endPos){
							cover = &(baseArr[pos-startPos].coverage);
							if(cover->idx_RefBase>=0 and cover->idx_RefBase<=4)
								cover->num_bases[cover->idx_RefBase] ++;
							else{
								cerr << __func__ << ", line=" << __LINE__ << ": invalid idx_RefBase " << cover->idx_RefBase << endl;
								outputAlnSegs(alnSegs);
								exit(1);
							}
						}
					break;
			case MD_MISMATCH:  // single base mismatch
					if((*seg)->startRpos>=startPos and (*seg)->startRpos<=endPos){
						cover = &(baseArr[(*seg)->startRpos-startPos].coverage);
						misbase = (*seg)->seg_MD.c_str()[0];
						switch(misbase){
							case 'a':
							case 'A': idx = 0; break;
							case 'c':
							case 'C': idx = 1; break;
							case 'g':
							case 'G': idx = 2; break;
							case 't':
							case 'T': idx = 3; break;
							case 'n':
							case 'N': idx = 4; break;
							default:
								cerr << __func__ << ", line=" << __LINE__ << ": invalid base " << int(misbase) << endl;
								exit(1);
						}
						if(idx!=cover->idx_RefBase) cover->num_bases[idx] ++;
						else if(idx==4) cover->num_bases[idx] ++;   // tolerate the mismatched base 'N'
						else{
							cerr << __func__ << ", line=" << __LINE__ << ": invalid array idx=" << idx << endl;
							exit(1);
						}
					}
				break;
			case BAM_CINS:  // insertion in query
				if((*seg)->startRpos>=startPos and (*seg)->startRpos<=endPos){
					if((*seg)->seglen>=min_ins_size_filt)
						baseArr[(*seg)->startRpos-startPos].addInsEvent(allocateInsEvent((*seg)->startRpos, (*seg)->seg_MD));
					else
						baseArr[(*seg)->startRpos-startPos].num_shortIns ++;
				}
				break;
			case BAM_CDEL:  // deletion in query
				if((*seg)->startRpos>=startPos and (*seg)->startRpos<=endPos){
					if((*seg)->seglen>=min_del_size_filt)
						baseArr[(*seg)->startRpos-startPos].addDelEvent(allocateDelEvent((*seg)->startRpos, (*seg)->seg_MD));
					else
						baseArr[(*seg)->startRpos-startPos].num_shortdel ++;
				}
				break;
			case BAM_CSOFT_CLIP:  // soft clipping in query
			case BAM_CHARD_CLIP:  // hard clipping in query
				if((*seg)->startRpos>=startPos and (*seg)->startRpos<=endPos){
					if((*seg)->startQpos==1) endflag = 0;
					else endflag = 1;
					baseArr[(*seg)->startRpos-startPos].addClipEvent(allocateClipEvent((*seg)->startRpos, (*seg)->opflag, endflag, (*seg)->seg_MD));
				}
				break;
			case BAM_CREF_SKIP:  // unexpected events
			case BAM_CPAD:
			case BAM_CEQUAL:
			case BAM_CDIFF:
			case BAM_CBACK:
				cerr << __func__ << ", line=" << __LINE__ << ": invalid opflag " << (*seg)->opflag << endl;
				exit(1);
				break;
			default:
				break;
		}
	}
	return 0;
}

// update the coverage information for each base in this block
void covLoader::updateBaseCovInfo(Base *baseArr){
	size_t pos;
	for(pos=startPos; pos<=endPos; pos++){
		baseArr[pos-startPos].insVector.shrink_to_fit();
		baseArr[pos-startPos].delVector.shrink_to_fit();
		baseArr[pos-startPos].clipVector.shrink_to_fit();
		baseArr[pos-startPos].updateCovInfo();
	}
}

// compute number of deletions
void covLoader::computeDelNumFromDelVec(Base *baseArr){
	Base *base;
	delEvent_t *del_event;

	size_t pos, pos_tmp, len;
	for(pos=startPos; pos<=endPos; pos++){
		base = baseArr + pos - startPos;
		for(size_t i=0; i<base->delVector.size(); i++){
			del_event = base->delVector.at(i);
			len = del_event->seq.size();
			for(size_t j=0; j<len; j++) {
				pos_tmp = pos + j;
				if(pos_tmp<=endPos) baseArr[pos_tmp-startPos].del_num_from_del_vec ++;
				else break;
			}
		}
	}
}

// output the aln seg items
void covLoader::outputAlnSegs(vector<struct alnSeg*> alnSegs){
	size_t i;
	vector<struct alnSeg*>::iterator seg;
	cout << "count: " << alnSegs.size() << endl;
	for(i=0, seg=alnSegs.begin(); seg!=alnSegs.end(); seg++, i++)
		cout << "[" << i << "]: startRpos=" << (*seg)->startRpos << ", startQpos=" << (*seg)->startQpos <<
				", seglen=" << (*seg)->seglen << ", opflag=" << size_t((*seg)->opflag) <<
				", seg_MD=" << (*seg)->seg_MD << endl;
}

// output the MD seg items
void covLoader::outputMDSeg(vector<struct MD_seg*> segs_MD){
	size_t i;
	vector<struct MD_seg*>::iterator seg;
	cout << "count: " << segs_MD.size() << endl;
	for(i=0, seg=segs_MD.begin(); seg!=segs_MD.end(); seg++, i++)
		cout << "[" << i << "]: seg=" << (*seg)->seg << ", opflag=" << size_t((*seg)->opflag) << endl;
}

// determine whether the alignment have the given cigar flag
bool covLoader::haveOpflagCigar(bam1_t* b, size_t opfalg){
	bool flag = false;
	uint32_t i, *c, op;

	for(i=0; i<b->core.n_cigar; i++){
		c = bam_get_cigar(b);  // CIGAR
		op = bam_cigar_op(c[i]);
		if(op==opfalg){ flag = true; break;}
	}
	return flag;
}
