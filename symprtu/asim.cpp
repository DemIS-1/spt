/* Assimilator: Process finished tasks and update segments. */
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
#include "bocom/Stream.cpp"

using std::vector;
using std::cerr;
using std::endl;
using std::string;

#include "primesieve.hpp"
#include "config.h"
#include "backend_lib.h"
#include "error_numbers.h"
#include "sched_config.h"
#include "sched_util.h"
#include "validate_util.h"
#include "credit.h"
#include "md5_file.h"
#include <mysql.h>

#include "wio.cpp"

//TODO: deduplicate this code
//#define DONT_CHANGE_TUPLES
#define DONT_TRUNC_GAPS
//demis
#ifndef ENABLE_SPT_RESULT_INSERT
#define ENABLE_SPT_RESULT_INSERT 1
#endif

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

#include "bocom/Wiodb.cpp"

class CFileStream
	: public CDynamicStream
{
	public:
	using CDynamicStream::CDynamicStream;
	void readFile( const char* name )
	{
		FILE* f = boinc_fopen(name, "r");
		if(!f) {
			//bug: boinc on windows is stupid and this call does not set errno if file does not exist
			//Go to hell!!
			if(errno==ENOENT) throw EFileNotFound();
			if(!boinc_file_exists(name)) throw EInvalid("no this file in root download directory."); //throw EFileNotFound();
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
DB_APP stpt_app;
MYSQL_STMT* spt_result_stmt;
std::vector<long> primes_small;
unsigned long spt_counters[3][64];

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
	if (stpt_app.lookup("where name='stpt'")) {
		std::cerr<<"can't find app stpt\n";
		exit(4);
	}

	#ifdef ENABLE_SPT_RESULT_INSERT
	{
		spt_result_stmt = mysql_stmt_init(boinc_db.mysql);
		char stmt[] = "insert ignore into spt_result SET id=?, input=?, output=?, batch=?, uid=?";
		if(mysql_stmt_prepare(spt_result_stmt, stmt, sizeof stmt ))
			throw EDatabase("spt_result insert prepare");
	}
	#endif
  primesieve::generate_primes(1600, &primes_small);
  cerr<<"Primes: "<<primes_small.size()<<" ^"<<primes_small.back()<<endl;

  memset(spt_counters, 0, sizeof spt_counters);
}

const float credit_m= 2.3148E-12* 16;

string getCurrentDateTime(bool useLocalTime) {
    std::stringstream currentDateTime;
    // current date/time based on current system
    time_t ttNow = time(0);
    tm * ptmNow;

    if (useLocalTime)
        ptmNow = localtime(&ttNow);
    else
        ptmNow = gmtime(&ttNow);

    currentDateTime << 1900 + ptmNow->tm_year <<"-";

    //month
    if (ptmNow->tm_mon < 9)
        //Fill in the leading 0 if less than 10
        currentDateTime << "0" << 1 + ptmNow->tm_mon<<"-";
    else
        currentDateTime << (1 + ptmNow->tm_mon)<<"-";

    //day
    if (ptmNow->tm_mday < 10)
        currentDateTime << "0" << ptmNow->tm_mday << " ";
    else
        currentDateTime <<  ptmNow->tm_mday << " ";

    //hour
    if (ptmNow->tm_hour < 10)
        currentDateTime << "0" << ptmNow->tm_hour<<":";
    else
        currentDateTime << ptmNow->tm_hour<<":";

    //min
    if (ptmNow->tm_min < 10)
        currentDateTime << "0" << ptmNow->tm_min<<":";
    else
        currentDateTime << ptmNow->tm_min<<":";

    //sec
    if (ptmNow->tm_sec < 10)
        currentDateTime << "0" << ptmNow->tm_sec;
    else
        currentDateTime << ptmNow->tm_sec;

    return currentDateTime.str();
}

bool check_prime(const uint64_t n)
{
	if(n<2) return false;
	for( const auto& p : primes_small ) {
		if( p >= n )
			return true;
		if(0==( n % p ))
			return false;
	}
	return true;
}

void check_symm_primes(const vector<TOutputTuple>& tuples, short mino, short mine)
{
	for( const auto& tuple : tuples) {
		if((tuple.k&1) && tuple.k<mino)
			continue;
		if(!(tuple.k&1) && tuple.k<mine)
			continue;
		if(tuple.k==0||tuple.k>64)
			throw EInvalid("bad tuple k");
		if(!check_prime(tuple.start))
			throw EInvalid("check_symm_primes composite start");
		unsigned d =0;
		if( (tuple.k&1) && tuple.ofs.size()!=(tuple.k/2) && tuple.ofs.size()!=((tuple.k/2)+1))
			throw EInvalid("check_symm_primes: invalid ofs size");
		if((tuple.k&1) && tuple.ofs.size()>(tuple.k/2) && tuple.ofs[tuple.ofs.size()-1] != tuple.ofs[tuple.ofs.size()-2])
			throw EInvalid("check_symm_primes: not symmetric odd");
		for(unsigned i=0; i<(tuple.k-1); ++i) {
			// 36 36 26 28 14 18 10 2  k=16, k/2=8
			//  0  1  2  3  4  5  6 7
			// 14 13 12 11 10  9  8
			// 30 30 24 36  k=9, k/2=4
			//  0  1  2  3
			//  7  6  5  4
			if(i>=(tuple.k/2))
				d += tuple.ofs[ tuple.k-i-2 ];
			else {
				d += tuple.ofs[i];
				if( (tuple.ofs[i]<=1) // must be >2
					||(tuple.ofs[i]&1)  // must be even
				) throw EInvalid("bad tuple offset");
			}
			if(!check_prime(tuple.start + d)) {
				cerr<<"check_symm_primes: "<<tuple.start<<"("<<tuple.k<<") ";
				for(auto a : tuple.ofs) cerr<<a<<" ";
				cerr<<"i="<<i<<" d="<<d<<endl;
				throw EInvalid("check_symm_primes composite");
			}
		}
	}
}

void check_twin_primes(const vector<TOutputTuple>& tuples)
{
	for( const auto& tuple : tuples) {
		if(!check_prime(tuple.start))
			throw EInvalid("check_twin_primes composite start");
		unsigned d =0;
		if(tuple.ofs.size()==0||tuple.k>64)
			throw EInvalid("bad twin tuple k");
		if(tuple.k!=(tuple.ofs.size()+1))
			throw EInvalid("bad twin ofs size");
		for(unsigned i=0; i<tuple.ofs.size(); ++i) {
			if( (tuple.ofs[i]<=1) // must be >2
				||(tuple.ofs[i]&1)  // must be even
			) throw EInvalid("bad twin offset");
			d += 2;
			if(!check_prime(tuple.start + d))
				throw EInvalid("check_twin_primes composite_2");
			d += tuple.ofs[i];
			if(!check_prime(tuple.start + d))
				throw EInvalid("check_twin_primes composite_d");
		}
	}
}

void result_validate(RESULT& result, CStream& input, TOutput output) {
	if(output.status!=TOutput::x_end)
		throw EInvalid("incomplete run");
	#ifndef DONT_CHANGE_TUPLES
	check_symm_primes(output.tuples, 11, 14);
	check_symm_primes(output.twin_tuples, 0, 10);
	check_twin_primes(output.twins);
	#endif
	check_twin_primes(output.twin_gap);
	//TODO: more consistency checks
}

static void insert_spt_tuple(const RESULT& result, const TOutputTuple& tuple, short kind, bool deriv)
{
	std::stringstream qr;
	qr=std::stringstream();
	qr<<"insert into spt set batch="<<result.batch;
	qr<<", start="<<tuple.start;
	qr<<", k="<<tuple.k;
	if(kind==0)
		qr<<", kind='spt'";
	else if(kind==1)
		qr<<", kind='stpt'";
	else if(kind==2)
		qr<<", kind='tpt'";
	qr<<", deriv="<<deriv<<"";
	qr<<", userid="<<result.userid;
	qr<<", resid="<<result.id;
	qr<<", ofs='"<<tuple.ofs[0];
	for(unsigned i=1; i<tuple.ofs.size(); ++i)
		qr<<" "<<tuple.ofs[i];
	qr<<"' on duplicate key update k=k";
	retval=boinc_db.do_query(qr.str().c_str());
	if(retval) throw EDatabase("spt row insert failed");
}
static void insert_spt_tuples(const RESULT& result, const vector<TOutputTuple>& tuples, short min_odd, short min_even)
{
	std::stringstream qr;
	for( const auto& tuple : tuples ) {
		short min = tuple.k &1 ? min_odd : min_even;
		spt_counters[!min_odd][tuple.k] += 1;
		if(tuple.k>=min)
			insert_spt_tuple(result, tuple, !min_odd, 0);
		TOutputTuple tu2= tuple;
		for( tu2.k= tuple.k-2; tu2.k>=min; tu2.k-=2 ) {
			tu2.start += tu2.ofs[0];
			tu2.ofs .erase(tu2.ofs.begin());
			/* new request: save stpt as spt as well,
			 * thus save as stpt if dealing with stpts and it has the 2 */
			if( min_odd || tu2.ofs[0]==2 ) { // truncate by 4 if STPT
				insert_spt_tuple(result, tu2, !min_odd, 1);
			}
		}
	}
}
static void insert_twin_tuples(const RESULT& result, const vector<TOutputTuple>& tuples, short min1)
{
	for( const auto& tuple2 : tuples) {
		spt_counters[2][tuple2.k] += 1;
		if(tuple2.k<min1)
			continue;
		TOutputTuple tuple = tuple2;
		tuple.k *=2;
		short min =min1 * 2;
		bool deriv=0;
		for(short left=0; 1; left++) {
			while(1) {
				insert_spt_tuple(result, tuple, 2, deriv);
				deriv=true;
				tuple.k -=2;
				if(tuple.k<min) break;
				tuple.ofs.resize(tuple.ofs.size()-1);
			}
			tuple.k = (tuple2.k-left-1)*2;
			if(tuple.k<min) break;
			tuple.start = tuple.start + tuple2.ofs[left];
			tuple.ofs = vector<unsigned short>( tuple2.ofs.begin()+left+1, tuple2.ofs.end() );
		}
	}
}

void result_insert(RESULT& result, TOutput output) {
	/* insert into the prime tuple db */
	#ifndef DONT_CHANGE_TUPLES
	short min_odd = 11;
	short min_even = 14;
	short min_twin = 6;
	short min_stpt = 10;
	if(output.start>=600000000000000000) {
		min_odd = 11;
		min_even = 16;
		//min_twin=7; ??
	}
	if(output.start>=2148001660000000000) {
		min_odd = 13;
		min_even = 16;
		min_stpt = 10;
		min_twin=7;
	}
	insert_spt_tuples(result, output.tuples, min_odd, min_even); // 11, 14
	insert_twin_tuples(result, output.twins, min_twin);
	insert_spt_tuples(result, output.twin_tuples, 0, min_stpt);
	insert_spt_tuples(result, output.twin_tuples, 63, min_even);
	#endif

	/* insert into largest gap table */
	/* check: find entry starting lower, but with larger d */
	/* post: find entry starting higher, but with smaller d */
	unsigned twin_gap_max = 2;
	for( const auto& tuple : output.twin_gap ) {
		short unsigned maxd =0;
		uint64_t start2 = tuple.start;
		for(auto d : tuple.ofs) {
			maxd= std::max(d,maxd);
			if( d > twin_gap_max ) {
				twin_gap_max = d;
				std::stringstream qr;
				qr<<"insert into spt_gap (start,d,resid,k,ofs) select "
				<<start2 <<','<<d <<','<<result.id <<",0,'";
				for(auto o : tuple.ofs)	qr<<" "<<o;
				qr<<"' from dual where not exists (select * from spt_gap where start<="
				<<start2 <<" and k=0 and d>="<<d <<");";
				retval=boinc_db.do_query(qr.str().c_str());
				if(retval) throw EDatabase("spt_gap insert select failed");
				qr=std::stringstream();
				qr<<"delete from spt_gap where k=0 and start>"
				<<start2 <<" and d<="<< d <<";";
				retval=boinc_db.do_query(qr.str().c_str());
				if(retval) throw EDatabase("spt_gap delete failed");
			}
			start2 = start2 + 2 + d;
		}
		if( tuple.k > 5) {
			std::stringstream qr;
			qr<<"insert into spt_gap (start,d,resid,k,ofs) select "
			<<start2 <<','<<maxd <<','<<result.id <<","<<tuple.k <<",'";
			for(auto o : tuple.ofs)	qr<<" "<<o;
			qr<<"' from dual where not exists (select * from spt_gap where start<="
			<<start2 <<" and k>5 and d>="<<maxd <<");";
			retval=boinc_db.do_query(qr.str().c_str());
			if(retval) throw EDatabase("spt_gap insert select failed");
			qr=std::stringstream();
			qr<<"delete from spt_gap where k>5 and start>"
			<<start2 <<" and d<="<< maxd <<";";
			retval=boinc_db.do_query(qr.str().c_str());
			if(retval) throw EDatabase("spt_gap delete failed");
		}
	}
}

void process_result(DB_RESULT& result) {
	std::stringstream qr;
	DB_WORKUNIT wu;
	if(wu.lookup_id(result.workunitid)) throw EDatabase("Workunit not found");
	// Read the result file
	CDynamicStream buf;
	retval=read_output_file_db(result,buf); // from bocom/Wiodb.cpp
	if(retval==ERR_FILE_MISSING) {
		retval=read_output_file(result,buf);
	}
	if(ERR_FILE_MISSING==retval) throw EInvalid("Output file absent");
	if(ERR_XML_PARSE==retval) throw EInvalid("Output file ERR_XML_PARSE");
	/* edit: skip processing if file error */
	if(retval && 1) {
		cerr<<"error: Can't read the output file. "<<result.name
		<<" "<<retval<<endl;
		return;
	}
	if(retval) throw EDatabase("can't read the output file");
	TOutput rstate;
	try {
		rstate.readOutput(std::move(buf));
	} catch (EStreamOutOufBounds& e){ throw EInvalid("can't deserialize output file"); }
	catch (std::length_error& e){ throw EInvalid("can't deserialize output file (bad vector length)"); }
	CFileStream inbuf;
	#ifdef ENABLE_SPT_RESULT_INSERT
	try {
		std::stringstream fn;
		fn<<config.download_dir<<"/"<<wu.name<<".in";
		inbuf.readFile( fn.str().c_str() );
	} catch (EStreamOutOufBounds& e){ throw EDatabase("can't read input file"); }
	#endif
	result_validate(result, inbuf, rstate);

	/* Insert into result db */
	#ifdef ENABLE_SPT_RESULT_INSERT
	unsigned long bind_2_length = inbuf.length();
	unsigned long bind_3_length = buf.length();
	MYSQL_BIND bind[] = {
		{.buffer=&result.id, .buffer_type=MYSQL_TYPE_LONG, 0},
		{.length=&bind_2_length, .buffer=inbuf.getbase(), .buffer_type=MYSQL_TYPE_BLOB, 0},
		{.length=&bind_3_length, .buffer=buf.getbase(), .buffer_type=MYSQL_TYPE_BLOB, 0},
		{.buffer=&result.batch, .buffer_type=MYSQL_TYPE_LONG, 0},
		{.buffer=&result.userid, .buffer_type=MYSQL_TYPE_LONG, 0}
	};
	if(mysql_stmt_bind_param(spt_result_stmt, bind))
		throw EDatabase("spt_result insert bind");
	if(mysql_stmt_execute(spt_result_stmt))
		throw EDatabase("spt_result insert");
	#endif

	result_insert(result, rstate);

	//TODO
	float credit = credit_m* (rstate.last-rstate.start);

	/*
	size_t logsz = strlen(result.stderr_out);
	snprintf(result.stderr_out+logsz,BLOB_SIZE-logsz,"Validator: OK! Log deleted to save space. "
		"ODLS=%lu CF=%lu SN=%llu ended=%d seg_dbid=%ld res_dbid=%lu\n",
		(unsigned long)rstate.odlk.size(),
		(unsigned long)rstate.nkf,
		(unsigned long long)rstate.nsn,
		(int)rstate.ended,
		have_segment? (long)segment.id : -1,
		(long)result_id
	);*/
	DB_HOST host;
	qr=std::stringstream();
	if(host.lookup_id(result.hostid)) throw EDatabase("Host not found");
	//is_valid
	result.server_state=RESULT_SERVER_STATE_OVER;
	result.outcome=RESULT_OUTCOME_SUCCESS;
	result.validate_state=VALIDATE_STATE_VALID;
	result.file_delete_state=FILE_DELETE_READY;
	double turnaround = result.received_time - result.sent_time;
	compute_avg_turnaround(host, turnaround);
	DB_HOST_APP_VERSION hav,hav0;
	retval = hav_lookup(hav0, result.hostid,
			generalized_app_version_id(result.app_version_id, result.appid)
	);
	hav=hav0;
	hav.max_jobs_per_day++;
	hav.consecutive_valid++;
	//grant_credit
	if(result.granted_credit==0) {
		result.granted_credit = credit;
		grant_credit(host, result.sent_time, result.granted_credit);
		if (config.credit_by_app) {
			grant_credit_by_app(result, credit);
		}
	}
	if(host.update()) throw EDatabase("Host update error");
	//update result (?)
	if(result.update()) throw EDatabase("Result update error");
	//update wu
	wu.assimilate_state = ASSIMILATE_DONE;
	//wu.file_delete_state=FILE_DELETE_READY;
	wu.need_validate = 0;
	wu.transition_time = time(0);
	//todo: unsent -> RESULT_OUTCOME_DIDNT_NEED
	if(wu.canonical_resultid==0) {
		wu.canonical_resultid = result.id;
		wu.canonical_credit = result.granted_credit;
	}
	if(wu.update()) throw EDatabase("Workunit update error");
	if (hav.host_id && hav.update_validator(hav0)) throw EDatabase("Host-App-Version update error");
}

void show_spt_counters()
{
	const char* label[] = {"SPT", "STPT", "TPT"};
	for(short kind=0; kind<3; ++kind) {
		short first, last;
		for(first=0; first<64 && !spt_counters[kind][first]; ++first);
		for(last=63; last>first && !spt_counters[kind][last]; --last);
		cerr<<"Count "<<label[kind]<<":";
		for(short i=first; i<=last; i++) {
			cerr<<" "<<i<<":"<<spt_counters[kind][i];
		}
		cerr<<endl;
	}
}

void process_ready_results(long gen_limit)
{
	//enumerate results
	std::stringstream enum_qr;
	//enum_qr<<"where appid="<<spt_app.id
	enum_qr<<"where appid in ("<<spt_app.id<<","<<stpt_app.id<<")"
	<<" and (( server_state="<<RESULT_SERVER_STATE_OVER
	<<" and outcome="<<RESULT_OUTCOME_SUCCESS<<" and validate_state="<<VALIDATE_STATE_INIT
	<<" ) or validate_state=9 ) limit "<<gen_limit<<";";
	DB_RESULT result;
	time_t t_begin = time(0);
	int count_rec=0;
	while(1) {
		int retval= result.enumerate(enum_qr.str().c_str());
		if(retval) {
			if (retval != ERR_DB_NOT_FOUND) {
				cerr<<"db error"<<endl;
				exit(4);
			}
			break;
		}
		cerr<<"result "<<result.name<<endl;
		try {
			process_result(result);
		} catch (EInvalid& e) {
			cerr<<" Invalid: "<<e.what()<<endl;
			strncat(result.stderr_out,"Validator: ",BLOB_SIZE-1);
			strncat(result.stderr_out,e.what(),BLOB_SIZE-1);
			set_result_invalid(result);
		}
		// Possible outcomes:
		// a) invalid - no credit, no results
		// b) error - unexpected error
		// c) valid - result saved, segment updated, credit granted
		// d) redundant/unnown - result saved, segment not found, credit granted -> valid
		//if( time(0) - t_begin > 30 ) {
		if( time(0) - t_begin > 50 ) {
			break;
		}
		count_rec++;
	}
	printf("Count added: %d\n",count_rec);
	show_spt_counters();
}

void database_reprocess(bool f_write)
{
	cerr<<"truncate tables...\n";
	#ifndef DONT_CHANGE_TUPLES
	retval=boinc_db.do_query("truncate table spt");
	if(retval) throw EDatabase("spt truncate failed");
	#endif
	#ifndef DONT_TRUNC_GAPS
	retval=boinc_db.do_query("truncate table spt_gap");
	if(retval) throw EDatabase("spt_gap truncate failed");
	#endif
	cerr<<"count...\n";
	long row_count;
	DB_BASE{"spt_result",&boinc_db}.count(row_count);
	cerr<<"Count: "<<row_count<<endl;

	DB_CONN enum_db;
	retval = enum_db.open(
			config.db_name, config.db_host, config.db_user, config.db_passwd
	);
	if (retval) throw EDatabase("Cant open second db connection.");

	retval=enum_db.do_query("select r.res, COALESCE(r.user,s.userid), COALESCE(r.batch,s.batch), "
		"w.data, r.data "
		"from result_file r join input_file w on r.wu=w.wu left join result s on r.res=s.id "
		"where r.app in(9,10,11) "); //TODO: use of constant
	if(retval) throw EDatabase("result_file enum query");
	MYSQL_RES* enum_res= mysql_use_result(enum_db.mysql);
	if(!enum_res) throw EDatabase("result_file enum use");

	RESULT result;
	long n_proc = 0;
	long n_inval =0;
	MYSQL_ROW enum_row;
	while(enum_row=mysql_fetch_row(enum_res)) {
		result.id= atol(enum_row[0]);
		result.userid= enum_row[1]? atol(enum_row[1]) : 0;
		result.batch= enum_row[2]? atol(enum_row[2]) : 0;
		unsigned long *enum_len= mysql_fetch_lengths(enum_res);
		try {
			cerr<<"\r"<<(n_proc+n_inval)<<" / "<<row_count<<" +inv"<<n_inval<<" #"<<result.id<<"               ";
			TOutput rstate;
			CStream res_inp_s((byte*)enum_row[3],enum_len[3]);
			try {
				rstate.readOutput(CStream((byte*)enum_row[4],enum_len[4]));
			} catch (EStreamOutOufBounds& e){ throw EInvalid("can't deserialize output file"); }
			catch (std::length_error& e){ throw EInvalid("can't deserialize output file (bad vector length)"); }

			result_validate(result, res_inp_s, rstate);
			result_insert(result, rstate);
			n_proc++;
		} catch (EInvalid& e) {
			cerr<<"Invalid: "<<e.what()<<endl;
			n_inval++;
		}
		#ifndef DONT_CHANGE_TUPLES
		if(0== n_proc % 500) {
			cerr<<endl;
			show_spt_counters();
		}
		#endif
		if(f_write && (0== n_proc % 10000)) {
			cerr<<"(commit)"<<endl;
			if(boinc_db.commit_transaction()) {
				cerr<<"Can't commit transaction!"<<endl;
				exit(1);
			}
			if(boinc_db.start_transaction())
				exit(4);
		}
	}
	if(retval=mysql_errno(enum_db.mysql)) {
		cerr<<"mysql_fetch_row error "<<retval<<" "<<mysql_error(enum_db.mysql)<<endl;
		throw EDatabase("fetch spt_result row");
	}
	mysql_free_result(enum_res);
	cerr<<endl<<"ok="<<n_proc<<" inval="<<n_inval<<endl;
	show_spt_counters();
}

int main(int argc, char** argv) {
	bool f_write, f_reproc;
	long gen_limit;
	int batchno;
	char *check1;
	if(argc!=3) {
			cerr<<"Expect 2 command line argument: f_write limit"<<endl;
			exit(2);
	}
	f_write = (argv[1][0]=='y');
	f_reproc = (argv[1][1]=='R');
	gen_limit = strtol(argv[2],&check1,10);
	if((argv[1][0]!='n' && !f_write && !f_reproc) || *check1 || (f_reproc&&!f_write)) {
			cerr<<"Invalid argument format"<<endl;
			exit(2);
	}

	time_t now = time(0);
	tm *ltm = localtime(&now);
	std::cout << getCurrentDateTime(ltm) << " Started\n" <<std::flush;

	cerr<<"f_write="<<f_write<<" f_reproc="<<f_reproc<<" limit="<<gen_limit<<endl;

	printf("ENABLE_SPT_RESULT_INSERT is:%d\n",ENABLE_SPT_RESULT_INSERT);

	//connect db if requested
	initz();
	if(boinc_db.start_transaction()){
		cerr<<"Can't start transaction! exit 4"<<endl;
		exit(4);
	}
	if(f_reproc){
		database_reprocess(f_write);
	} else {
		process_ready_results(gen_limit);
	}
	if(f_write) {
		if(boinc_db.commit_transaction()) {
			cerr<<"Can't commit transaction!"<<endl;
			exit(1);
		}
	}
	boinc_db.close();
	return 0;
}

/* ok=357052 inval=270
Count SPT: 9:1415304 10:0 11:320979 12:15032 13:6131 14:6598462 15:60 16:530544 17:1 18:25702 19:0 20:1314 21:0 22:69 23:0 24:4
Count STPT: 8:40655218 9:0 10:472777 11:0 12:22856 13:0 14:25 15:0 16:2
Count TPT: 6:2107451 7:98346 8:3084 9:88 10:2
* [Inferior 1 (process 66590) exited normally]
* ok=435932 inval=227
*/
