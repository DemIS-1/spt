<?php
/* spt/list.php - list found prime tuples from database */
require_once("../inc/util.inc");
require_once("../inc/boinc_db.inc");

ini_set('max_execution_time', 120);
//ini_set('memory_limit', '128M');
ini_set('memory_limit', '192M');
//set_time_limit(240);
$global_limit_query_records=200000;

function convert_twin_tuple($start,$k) {
	$a=array(0);
	$next_d=0;
	$next_prime=$start-1;
	for($i=0;$i<$k;$i++){
		if($i==0) {$a[$i]=0; continue;}
		$next_prime=gmp_nextprime($next_prime+1);
		$next_d=gmp_sub($next_prime,$start);
		$a[$i]=$next_d;
	}
	$astr=implode(' ',$a);
	return $astr;
}

function show_by_batch_group_k($f_batch) {
  $fmt=get_int('fmt',true);
  header("Content-type: text/plain");
  $db = BoincDb::get();
	global $global_limit_query_records;
	$query="SELECT DISTINCT(start), id, k, ofs, kind
		 FROM spt
		 WHERE kind in ('spt','stpt')
		 and batch=$f_batch
		 and k>12 and not k=14
		 ORDER BY k, start LIMIT 0,$global_limit_query_records";
	$set = $db->do_query($query);
  $prevk=0;
  echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.\n";
	echo "# where batch = $f_batch limit 0,$global_limit_query_records\n";
	$lastid=-1; $rcount=0;
  while($row = $set->fetch_object('stdClass')) {
		if($fmt==3) {
			$astr = "k={$row->k} {$row->ofs}";
		} else {
			$astr=convert_twin_tuple($row->start,$row->k);
		}
		if($row->k!=$prevk) {
			echo "k=$row->k\n";
			$prevk=$row->k;
		}
    echo "{$row->start}: $astr\n";
    if($row->id>$lastid) $lastid=$row->id; $rcount++;
  }
  echo "# last = $lastid\n";
  $set->free();
}

function show_by_k() {
	$k= get_int('k');
	$minid = get_int('i',true);
  header("Content-type: text/plain");
  $db = BoincDb::get();
  $minidclausule = "";
  if($minid!==null)
		$minidclausule = "and id>$minid";
	global $global_limit_query_records;
	$query="SELECT DISTINCT(start), id, k, ofs, kind
		 FROM spt
		 WHERE kind in ('spt','stpt')
		 AND k=$k $minidclausule
		 AND k>12 and not k=14
		 ORDER BY start LIMIT 0,$global_limit_query_records ;";
	$set = $db->do_query($query);
  $prevk=0;
  echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.\n";
	echo "# where k=$k limit 0,$global_limit_query_records\n";
  if($minid!==null)
		echo "# where id > $minid\n";
	$lastid=-1; $rcount=0;
  $prev=0;
  while($row = $set->fetch_object('stdClass')) {
    if($row->start==$prev)
      continue;
    $prev=$row->start;
	$astr=convert_twin_tuple($row->start,$row->k);
    echo "{$row->start}: $astr\n";
    if($row->id>$lastid) $lastid=$row->id; $rcount++;
  }
  echo "# last = $lastid # count = $rcount\n";
  $set->free();
}

function show_by_tuples($f_mode_par,$f_type_par,$f_num_page_par,$f_ln_par) {
  $fmt=get_int('fmt',true);
  if(null!==$f_ln_par) {
	header("Content-type: text/html");
	$br="<br>";
  }else{
	$br="";
	header("Content-type: text/plain");
  }
  $db = BoincDb::get();
  echo "# page=$f_num_page_par, count=(?), batch=(?)$br\n";
	global $global_limit_query_records;
	$query="SELECT distinct(spt.`start`), k, ofs, kind
		 FROM spt
		 WHERE (spt.`start`>=0 or spt.`start`<= 1898337310272981169)
			 AND spt.kind='$f_mode_par'
			 AND spt.k=$f_type_par
			 ORDER BY spt.`start` LIMIT 0,$global_limit_query_records; ";
  $set = $db->do_query($query);
  $prevk=0;
  if($br=="") {
    echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomash Brada, ask on forum about reuse or citation.$br\n";
  }else{
    echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.$br\n";
  }
        echo "# where `start`>=0 and `start`<= 1898337310272981169 and kind='$f_mode_par' and k=$f_type_par limit 0,$global_limit_query_records$br\n";
        $lastid=-1; $rcount=0;
  while($row = $set->fetch_object('stdClass')) {
                if($fmt==3) {
                        $astr = "k={$row->k} {$row->ofs}";
                } else {
			$astr=convert_twin_tuple($row->start,$row->k);
                }
                if($row->k!=$prevk) {
                        $prevk=$row->k;
                }
    if($br=="") {
      echo "{$row->start}: $astr\n";
    }else{
      echo "<a href='spt_explore.php?{$row->kind}={$row->k}&s={$row->start}'>{$row->start}</a>: $astr$br\n";
    }
    if($row->start>$lastid) $lastid=$row->start; $rcount++;
  }
  echo "# count = $rcount<br>\n";
  $set->free();
}


function main() {
  if(null!==$f_batch=get_int('batch',true)) {
		show_by_batch_group_k($f_batch);
	}else if(null!==get_int('k',true)) {
		show_by_k();
	}else if((null!==$f_type=get_str('tpt',true))&&(null!==$f_num_page=get_int('p',true))&&(null!==$f_ln=get_str('ln',true))) {
		echo "1 tpt pages#$f_type , $f_num_page ,  $f_ln (and is ln mode on)\n";
		show_by_tuples("tpt",$f_type,$f_num_page,$f_ln);
	}else if((null!==$f_type=get_str('stpt',true))&&(null!==$f_num_page=get_int('p',true))&&(null!==$f_ln=get_str('ln',true))) {
		echo "2 stpt pages#$f_type , $f_num_page ,  $f_ln (and is ln mode on)\n";
	}else if((null!==$f_type=get_str('spt',true))&&(null!==$f_num_page=get_int('p',true))&&(null!==$f_ln=get_str('ln',true))) {
		show_by_tuples("spt",$f_type,$f_num_page,$f_ln);
	}else if((null!==$f_type=get_str('spt',true))&&(null!==$f_num_page=get_int('p',true))) {
		show_by_tuples("spt",$f_type,$f_num_page,$f_ln);
	}else if((null!==$f_type=get_int('spt',true))&&(null!==$f_ln=get_str('ln',true))) {
		echo "4 spt no pages#$f_type (is ln only mode on)\n";
	}else{
		$a=0;
		echo "any others#else last\n";
	}
}

main();
?>
