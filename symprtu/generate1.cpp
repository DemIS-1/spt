/* Generator */
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <bitset>
#include <ctime>
#include <unistd.h>
#include <cstdlib>
#include <sys/stat.h>

#include "boinc_api.h"
#include "../bocom/Stream.cpp"

using std::vector;
using std::cerr;
using std::endl;

#include "config.h"
#include "backend_lib.h"
#include "error_numbers.h"
#include "sched_config.h"
#include "sched_util.h"
#include "validate_util.h"
#include "credit.h"
#include "md5_file.h"

#include "wio.cpp"

struct EApp : std::runtime_error { using std::runtime_error::runtime_error; };
struct EBoincApi : std::exception {
	int retval;
	const char* msg;
	std::string msg2;
	EBoincApi(int _retval, const char* _msg)
		:retval(_retval), msg(_msg)
	{
		std::stringstream ss;
		ss<<"boinc_api: "<<msg<<": "<<boincerror(retval);
		msg2=ss.str();
	}
	const char * what () const noexcept {return msg2.c_str();}
};
struct EDatabase	: std::runtime_error { using runtime_error::runtime_error; };
struct EInvalid	: std::runtime_error { using runtime_error::runtime_error; };
static int retval;

#include "../bocom/Wiodb.cpp"

class CFileStream
	: public CDynamicStream
{
	public:
	using CDynamicStream::CDynamicStream;
	CFileStream( const char* name )
	{
		FILE* f = boinc_fopen(name, "r");
		if(!f) {
			//bug: boinc on windows is stupid and this call does not set errno if file does not exist
			//Go to hell!!
			if(errno==ENOENT) throw EFileNotFound();
			if(!boinc_file_exists(name)) throw EFileNotFound();
			throw std::runtime_error("fopen");
		}
		struct stat stat_buf;
		if(fstat(fileno(f), &stat_buf)<0)
			throw std::runtime_error("fstat");
		this->setpos(0);
		this->reserve(stat_buf.st_size);
		if( fread(this->getbase(), 1, stat_buf.st_size, f) !=stat_buf.st_size)
			throw std::runtime_error("fread");
		this->setpos(0);
		fclose(f);
	}
	void writeFile( const char* name )
	{
		FILE* f = boinc_fopen("tmp_write_file", "w");
		if(!f)
			throw std::runtime_error("fopen");
		if( fwrite(this->getbase(), 1, this->pos(), f) !=this->pos())
			throw std::runtime_error("fwrite");
		fclose(f);
		if( rename("tmp_write_file",name) <0)
			throw std::runtime_error("rename");
	}
};

DB_APP spt_app;
const char spt_template [] =
"<input_template>\n"
"<file_info>\n"
"<number>0</number>\n"
"</file_info>\n"
"<workunit>\n"
"<file_ref>\n"
"<file_number>0</file_number>\n"
"<open_name>input.dat</open_name>\n"
"</file_ref>\n"
"</workunit>\n"
"</input_template>\n";

int batch=0, delay_bound=0, min_quorum=0, target_nresults=0, max_error_results=0, max_total_results=0, max_success_results=0, wu_in_file_generate=0,batch_post_forum=0;
double memory_bound=0,disk_bound=0;
uint64_t start=0, end=0, step=0, next=0;
unsigned maxcnt=0;
unsigned long count=0;
bool b_wu_in_file_generate;
bool b_batch_post_forum;
char conf_from[20];
char msg_p1[256];

void initz() {
	int retval = config.parse_file();
	if (retval) {
			log_messages.printf(MSG_CRITICAL,
					"Can't parse config.xml: %s\n", boincerror(retval)
			);
			exit(1);
	}

	retval = boinc_db.open(
			config.db_name, config.db_host, config.db_user, config.db_passwd
	);
	if (retval) {
			log_messages.printf(MSG_CRITICAL,
					"boinc_db.open failed: %s\n", boincerror(retval)
			);
			exit(1);
	}
	if (spt_app.lookup("where name='spt'")) {
		std::cerr<<"can't find app spt\n";
		exit(4);
	}
	//demis config beg
	std::ifstream fin("config_generate1.txt");
	if(!fin){throw EDatabase("No file config_generate1.txt");}
	if(!(fin >> batch >> start >> end >> step >> next >> maxcnt >> count >> delay_bound >> memory_bound >> disk_bound >> min_quorum >> target_nresults >> max_error_results >> max_total_results >> max_success_results >> wu_in_file_generate >> batch_post_forum >> conf_from)){throw EDatabase("Incorrect format data in config_generate1.txt");}
	if(batch < 1 || start < 1 || end < 1 || step < 1 ||maxcnt < 1 || count < 0 || delay_bound < 1 || memory_bound < 1 || disk_bound < 1 || min_quorum < 1 || target_nresults < 1 || max_error_results < 1 || max_total_results < 1 || max_success_results < 1 || wu_in_file_generate < -256 || batch_post_forum < -256 ){cerr << "Incorrect format data config_generate1.txt\n"; fin.close(); printf("check config_generate1.txt. exit(1)\n");exit(1);}
	fin.close();
	if(wu_in_file_generate > 0){
		b_wu_in_file_generate=true;
		std::cout<<"generate file spt_"<<batch<<"_*.txt mode is ON.\n";
	}else{
		std::cout<<"no generate file spt_"<<batch<<"_*.txt mode.\n";
	}
	if(batch_post_forum > 0){
		b_batch_post_forum=true;
		std::cout<<"post message to forum is ON.\n";
	}else{
		std::cout<<"post message to forum is OFF.\n";
	}
	//end config
}

void post_batch_msg(int batch,uint64_t first,uint64_t next,unsigned long count, const char* label, const char * msg)
{
	//into message
	std::stringstream post;
	  post<<"insert into post set thread= 6, user= 6, timestamp= UNIX_TIMESTAMP(), modified= 0, parent_post= 0, score= 0, votes= 0, signature= 1, hidden= 0, content='";
	post<<"Batch "<<batch<<": "<<first<<" .. "<<next<<" -1\nCount: "<<count<<"\n";
	post<<msg;
	post<<"';";
	retval=boinc_db.do_query(post.str().c_str());
	if(retval) throw EDatabase("batch forum post insert failed");
	DB_ID_TYPE message_id = boinc_db.insert_id();
	std::stringstream qr;
	//into batch
	qr<<"insert into batch set user_id= 6, create_time= UNIX_TIMESTAMP(), logical_start_time= 0, logical_end_time= 0, est_completion_time= 0, njobs= 0, fraction_done= 0.1, nerror_jobs= 0, state= 0, completion_time= 0, credit_estimate= 0, credit_canonical= 0, credit_total= 0, name= 'spt-ut', app_id= 5, project_state= 1, description= 'descr test for', expire_time= 0, id= "<<batch<<", short_descr= '"<<label<<"', forum_msg= "<<message_id<<";";
	retval=boinc_db.do_query(qr.str().c_str());
	if(retval) throw EDatabase("batch descr insert failed");
}

void check_wu(){
// need check if data wu exist in download, in DB: workunit, input_file, batch and ...

}

void submit_wu_in(uint64_t start, uint64_t end, int batch)
{
	std::stringstream wuname;
	DB_WORKUNIT wu; wu.clear();
	TInput inp;

		inp.start= start;
		inp.end= end;

		inp.mine_k= 16;
		inp.mino_k= 13;
		inp.max_k= 64;
		inp.upload = 0;
		inp.exit_early= 0;
		inp.out_last_primes= 0;
		inp.out_all_primes= 0;
		inp.primes_in.clear();
		inp.twin_k=7;
		inp.twin_min_k=10;
		inp.twin_gap_k=6;
		inp.twin_gap_min=886; //1300
		inp.twin_gap_kmin=400; //766

		wu.appid = spt_app.id;
		//14e12 is one hour on mangan-pc
		wu.rsc_fpops_est = (inp.end - inp.start) * 14; //what is???
		wu.rsc_fpops_bound = wu.rsc_fpops_est * 24; //what is???
		//wu.rsc_memory_bound = 671e6;
		wu.rsc_memory_bound = memory_bound;
		//wu.rsc_disk_bound = 1e8; //todo 100m
		wu.rsc_disk_bound = disk_bound; //todo 100m
		//wu.delay_bound = 6 * 24 * 3600; //518400 or 6 day wait answer from client
		wu.delay_bound = delay_bound; //518400 or 6 day wait answer from client
		wu.priority = 22;
		wu.batch= batch;
//		wu.min_quorum = 2;
		wu.min_quorum = min_quorum;
//		wu.target_nresults= 4;// target_nresults=min_quorum or > min_quorum
		wu.target_nresults= target_nresults;// target_nresults=min_quorum or > min_quorum
//		wu.max_error_results= 8;
		wu.max_error_results= max_error_results;
//		wu.max_total_results= 6;
		wu.max_total_results= max_total_results;
//		wu.max_success_results= 2;
		wu.max_success_results= max_success_results;
	wuname<<"spt_"<<wu.batch<<"_"<<inp.start;
	std::cout<<" WU "<<wuname.str()<<" "<<inp.end<<endl;
	strcpy(wu.name, wuname.str().c_str());
	CDynamicStream input_buf;
	inp.writeInput(input_buf);
	if(b_wu_in_file_generate){
		CFileStream buf;
		inp.writeInput(buf);
		std::stringstream fninp;
		fninp<<config.download_dir<<"/"<<wuname.str()<<".in";
		try{
			buf.writeFile(fninp.str().c_str());
		}
		catch(std::runtime_error& e) {
			throw EDatabase("Unable to write next input file");
		}
		vector<INFILE_DESC> infile_specs{1};
		infile_specs[0].is_remote = false;
		strcpy(infile_specs[0].name, (wuname.str()+".in").c_str());
		retval= create_work2(wu, spt_template,"templates/spt_out",0,infile_specs,config,0,0,0);
		if(retval) throw EDatabase("create_work2 failed");
	}else{
		retval= create_work3(wu, "templates/spt_out", config, input_buf,1);
		if(retval) throw EDatabase("create_work3 failed");
	}
}

int main(int argc, char** argv) {

	batch=0;
	//node: min app version is set in app table
	initz();
	if(boinc_db.start_transaction()){
		std::cout<<"no boinc_db.start_transaction. exit(4).\n";
		exit(4);
	}
	next = start;
	while(1) {
		uint64_t curr = next;
		if(curr > end){
			std::cout<<"curr > end, fiels 2 and 3 in in config_generate.txt. break. it's no err\n";
			break;
		}
		if(count>=maxcnt){
			std::cout<<"count>=maxcnt. count="<<count<<" maxcnt="<<maxcnt<<" . field 6 and 7 in config_generate.txt. break. it's no err.\n";
			break;
		}
		next = curr + step;
		submit_wu_in(curr, next, batch);
		count++;
	}
	if(b_batch_post_forum){
		strcat(msg_p1,"Continue from ");
		strcat(msg_p1,conf_from);
		post_batch_msg(batch,start,next,count,"spt-ut",msg_p1);
	}

	cerr<<"Count: "<<count<<endl;
	cerr<<"First: "<<start<<endl;
	cerr<<"Next : "<<next<<endl;

	if(boinc_db.commit_transaction()) {
		cerr<<"failed to commit transaction. exit(1)\n"<<endl;
		exit(1);
	}
	boinc_db.close();
	return 0;
}

