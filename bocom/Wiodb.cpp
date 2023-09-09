
int create_work4(
    DB_WORKUNIT& wu,
    const char* result_template_filename,
    SCHED_CONFIG& config_loc
) {
    int retval;

    wu.create_time = time(0);

    // check for presence of result template.
    // we don't need to actually look at it.
    //
    const char* p = config_loc.project_path(result_template_filename);
    if (!boinc_file_exists(p)) {
        fprintf(stderr,
            "create_work: result template file %s doesn't exist\n", p
        );
        return retval;
    }

    if (strlen(result_template_filename) > sizeof(wu.result_template_file)-1) {
        fprintf(stderr,
            "result template filename is too big: %d bytes, max is %d\n",
            (int)strlen(result_template_filename),
            (int)sizeof(wu.result_template_file)-1
        );
        return ERR_BUFFER_OVERFLOW;
    }
    strncpy(wu.result_template_file, result_template_filename, sizeof(wu.result_template_file));

    if (wu.rsc_fpops_est == 0) {
        fprintf(stderr, "no rsc_fpops_est given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.rsc_fpops_bound == 0) {
        fprintf(stderr, "no rsc_fpops_bound given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.rsc_disk_bound == 0) {
        fprintf(stderr, "no rsc_disk_bound given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.target_nresults == 0) {
        fprintf(stderr, "no target_nresults given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.max_error_results == 0) {
        fprintf(stderr, "no max_error_results given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.max_total_results == 0) {
        fprintf(stderr, "no max_total_results given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.max_success_results == 0) {
        fprintf(stderr, "no max_success_results given; can't create job\n");
        return ERR_NO_OPTION;
    }
    if (wu.max_success_results > wu.max_total_results) {
        fprintf(stderr, "max_success_results > max_total_results; can't create job\n");
        return ERR_INVALID_PARAM;
    }
    if (wu.max_error_results > wu.max_total_results) {
        fprintf(stderr, "max_error_results > max_total_results; can't create job\n");
        return ERR_INVALID_PARAM;
    }
    if (wu.target_nresults > wu.max_success_results) {
        fprintf(stderr, "target_nresults > max_success_results; can't create job\n");
        return ERR_INVALID_PARAM;
    }
    /*
    auto prev_transitioner_flags= wu.transitioner_flags;
    wu.transitioner_flags= 1;
    */
    if (wu.transitioner_flags) {
        wu.transition_time = INT_MAX;
    } else {
        wu.transition_time = time(0);
    }

//temporary blocked
		retval = wu.insert();
		if (retval) {
				fprintf(stderr,
						"create_work4: workunit.insert() %s\n", boincerror(retval)
				);
				return retval;
		}
//temporary blocked
		wu.id = boinc_db.insert_id();

		/*
		wu.transitioner_flags= prev_transitioner_flags;
    if (wu.transitioner_flags) {
        wu.transition_time = INT_MAX;
    } else {
        wu.transition_time = time(0);
    }
		wu.update();
		*/

    return 0;
}

int create_work3(
    DB_WORKUNIT& wu,
    const char* result_template_filename,
        // relative to project root; stored in DB
    SCHED_CONFIG& config_loc,
    const CStream& input_data,
    int mode_generate
) {
    int retval;

    unsigned long in_len = input_data.pos();
    char in_md5[256];
    md5_block((const unsigned char*)input_data.getbase(), in_len, in_md5);
	if(mode_generate == 1){
	    snprintf(wu.xml_doc, sizeof(wu.xml_doc),
	    "<file_info>\n"
	    "<name>%s.in</name>\n"
	    "<url>https://boinc.termit.me/adsl_cgi/file_upload_handler?%s.in</url>\n"
	    "<md5_cksum>%s</md5_cksum>\n"
	    "<nbytes>%lu</nbytes>\n"
	    "</file_info>\n"
	    "<workunit>\n"
	    "<file_ref>\n"
	    "<file_name>%s.in</file_name>\n"
	    "<open_name>input.dat</open_name>\n"
	    "</file_ref>\n"
	    "</workunit>\n"
	    , wu.name, wu.name
	    , in_md5, in_len
	    , wu.name
	    );
	}
	if(mode_generate == 22){
		// mode=2 is file generate method
		snprintf(wu.xml_doc, sizeof(wu.xml_doc),
		"<file_info>\n"
		"<name>%s.in</name>\n"
		"<md5_cksum>%s</md5_cksum>\n"
		"<nbytes>%lu</nbytes>\n"
		"</file_info>\n"
		"<workunit>\n"
		"<file_ref>\n"
		"<file_name>%s.in</file_name>\n"
		"<open_name>input.dat</open_name>\n"
		"</file_ref>\n"
		"</workunit>\n"
		, wu.name
		, in_md5, in_len
		, wu.name
		);
	}

//2023-07-11
	if(mode_generate == 1){
		retval = create_work4(wu, result_template_filename, config_loc);
                std::cout<<"create_work4() filed. make retval now..\n";
                return retval;
	}
//    if(retval){
//      std::cout<<"create_work4() filed. make retval now..\n";
//      return retval;
//    }

    //insert input_file
    MYSQL_STMT* insert_stmt = 0;
    insert_stmt = mysql_stmt_init(boinc_db.mysql);
    char stmt[] = "insert into input_file SET wu=?, data=?";
    void* in_data= (void*)input_data.getbase();
    MYSQL_BIND bind[] = {
		    {.buffer=&wu.id, .buffer_type=MYSQL_TYPE_LONG, 0},
		    {.length=&in_len, .buffer=in_data, .buffer_type=MYSQL_TYPE_BLOB, 0},
    };
    if(!insert_stmt
		    || mysql_stmt_prepare(insert_stmt, stmt, sizeof stmt )
		    || mysql_stmt_bind_param(insert_stmt, bind)
		    || mysql_stmt_execute(insert_stmt)
    ) {
		    mysql_stmt_close(insert_stmt);
		    fprintf(stderr,
				    "create_work3: insert of input_data failed %s\n", mysql_error(boinc_db.mysql) );
		    //wu.delete_from_db();
		    return -1;
    }
    mysql_stmt_close(insert_stmt);

    return 0;
}

int read_output_file(RESULT const& result, CDynamicStream& buf) {
    char path[MAXPATHLEN];
		path[0]=0;
    std::string name;
		double usize = 0;
		double usize_max = 0;
    MIOFILE mf;
    mf.init_buf_read(result.xml_doc_out);
    XML_PARSER xp(&mf);
    while (!xp.get_tag()) {
			if (!xp.is_tag) continue;
			if (xp.match_tag("file_info")) {
				while(!xp.get_tag()) {
					if (!xp.is_tag) continue;
					if(xp.parse_string("name",name)) continue;
					if(xp.parse_double("nbytes",usize)) continue;
					if(xp.parse_double("max_nbytes",usize_max)) continue;
					if (xp.match_tag("/file_info")) {
						if(!name[0] || !usize) {
							return ERR_XML_PARSE;
						}
						dir_hier_path(
							name.c_str(), config.upload_dir,
							config.uldl_dir_fanout, path
						);

						FILE* f = boinc_fopen(path, "r");
						if(!f && ENOENT==errno) return ERR_FILE_MISSING;
						if(!f) return ERR_READ;
						struct stat stat_buf;
						if(fstat(fileno(f), &stat_buf)<0) return ERR_READ;
						buf.setpos(0);
						buf.reserve(stat_buf.st_size);
						if( fread(buf.getbase(), 1, stat_buf.st_size, f) !=stat_buf.st_size)
							return ERR_READ;
						buf.setpos(0);
						fclose(f);
						return 0;
					}
				}
			}
		}
    return ERR_XML_PARSE;
}

int read_output_file_db(RESULT const& result, CDynamicStream& buf) {
	char sql[MAX_QUERY_LEN];
	sprintf(sql, "select id, data from result_file where res='%lu' order by id desc limit 1", result.id);
	int retval=boinc_db.do_query(sql);
	if(retval) return retval;
	MYSQL_RES* enum_res= mysql_use_result(boinc_db.mysql);
	if(!enum_res) return -1;
	MYSQL_ROW row=mysql_fetch_row(enum_res);
	if (row == 0) {
		mysql_free_result(enum_res);
		return ERR_FILE_MISSING;
	}
	unsigned long *enum_len= mysql_fetch_lengths(enum_res);
	buf.setpos(0);
	//buf.reserve(enum_len[1]);
	buf.write(row[1], enum_len[1]);
	buf.setpos(0);
	mysql_free_result(enum_res);
	return 0;
}

void set_result_invalid(DB_RESULT& result) {
	DB_WORKUNIT wu;
	if(wu.lookup_id(result.workunitid)) throw EDatabase("Workunit not found");
	DB_HOST_APP_VERSION hav, hav0;
	retval = hav_lookup(hav0, result.hostid,
			generalized_app_version_id(result.app_version_id, result.appid)
	);
	hav= hav0;
	hav.consecutive_valid = 0;
	if (hav.max_jobs_per_day > config.daily_result_quota) {
			hav.max_jobs_per_day--;
	}
	result.validate_state=VALIDATE_STATE_INVALID;
	result.outcome=6;
	wu.transition_time = time(0);
	//result.file_delete_state=FILE_DELETE_READY; - keep for analysis
	if(result.update()) throw EDatabase("Result update error");
	if(wu.update()) throw EDatabase("Workunit update error");
	if (hav.host_id && hav.update_validator(hav0)) throw EDatabase("Host-App-Version update error");
}
