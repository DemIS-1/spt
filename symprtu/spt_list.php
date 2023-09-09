<?php
/* spt/list.php - list found prime tuples from database */
require_once("../inc/util.inc");
require_once("../inc/boinc_db.inc");

ini_set('max_execution_time', 120);
//set_time_limit(240);
ini_set('memory_limit', '192M');
$global_limit_query_records=200000;

function convert_tuple($ofs,$k) {
	$d= explode(' ',$ofs);
	$d=array_pad($d,$k-1,0);
	//mirror d
	for($i=0,$j=$k-2;$i<$j;$i++,$j--) {
		$d[$j]=$d[$i];
	}
	$a=array(0);
	for($i=1;$i<$k;$i++) {
		$a[$i] = $a[$i-1] + $d[$i-1];
	}
	$astr=implode(' ',$a);
	$dstr=implode(' ',$d);
	return $astr;
}

function convert_twin_tuple($start,$k) {
	$a=array(0);
	$next_d=0;
	$next_prime=$start-1;
	for($i=0;$i<$k-1;$i++){
		$next_prime=gmp_nextprime($next_prime+1);
		$next_d=gmp_sub($next_prime,$start);
		$a[$i]=$next_d;
	}
	$a=array_pad($a,(count($a)+1) * -1,0);
	$astr=implode(' ',$a);
	return $astr;
}

function show_by_batch_group_k($f_batch) {
  $fmt=get_int('fmt',true);
  header("Content-type: text/plain");
  $db = BoincDb::get();
  $set = $db->do_query("select distinct(start), id, k, ofs from spt where kind in ('spt','stpt') and batch=$f_batch and k>12 and not k=14 order by k, start limit 0,200000 ");
  $prevk=0;
  echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.\n";
	echo "# where batch = $f_batch limit 0,200000 \n";
        echo "# (temporary limit row to output, by Demis. I will need to make a window type query for large data ranges.)\n";
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
  header("Content-type: text/html");
  $db = BoincDb::get();
  $minidclausule = "";
  if($minid!==null)
		$minidclausule = "and id>$minid";
	$query="SELECT count(*) as Count
		 FROM spt
		 WHERE kind in ('spt','stpt')
		 AND k=$k $minidclausule";
	$set_get_cnt = $db->do_query($query);
	$row = $set_get_cnt->fetch_object('stdClass'); // ^
	$sumR=$row->Count;
	global $global_limit_query_records;
	$max_per_page = $global_limit_query_records;
  $pages=ceil($sumR/$max_per_page);
  $p = get_int('p');
  if(($p<$pages)||($p>$pages)) $p=1;
  if($p==1) $start_p=0; else $start_p=($max_per_page*$p)-$max_per_page;
  if($p>$max_per_page/10) $start_p=0;
	$query="SELECT DISTINCT(start), id, k, ofs
		 FROM spt
		 WHERE kind in ('spt','stpt')
		 AND k=$k $minidclausule
		 ORDER BY start limit $start_p,$max_per_page";
	$set = $db->do_query($query);
  $prevk=0;
  echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.<br>\n";
	echo "# where k = $k limit $start_p,$max_per_page<br>\n";
	echo"# (temporary limit row to output, by Demis. I will need to make a window type query for large data ranges.)<br>\n";
  if($minid!==null)
		echo "# where id > $minid\n";
	$lastid=-1; $rcount=0;
  $prev=0;
  while($row = $set->fetch_object('stdClass')) {
    if($row->start==$prev)
      continue;
    $prev=$row->start;
		$astr=convert_twin_tuple($row->start,$row->k);
    echo "{$row->start}: $astr<br>\n";
    if($row->id>$lastid) $lastid=$row->id; $rcount++;
  }
  echo "# last = $lastid # count = $rcount<br>\n";
  echo "# count = $rcount<br>\n";
  if($pages>1){
    echo "# <a href=".$_SERVER['PHP_SELF']."?qq=1&k=$k&p=" . ($p+1) . "&ln>next</a><br>\n";
  }else{
	$a=1;
  }
  $set_get_cnt->free();
  $set->free();
}

function main() {
	if(null!==$f_batch=get_int('batch',true)) {
		show_by_batch_group_k($f_batch);
	} else if (null!==get_int('k',true)) {
		show_by_k();
	}else{
		echo "Unknown query...<br><br>\n";
		echo "Use <a href=\"".$_SERVER['PHP_SELF']."?batch=97\"> link</a> for 'batch' result<br>\n";
		echo "or use <a href=\"".$_SERVER['PHP_SELF']."?k=15&p=1\"> link</a> for 'k' result<br>\n";
		echo "or use <a href=\"tpt_gaps.php?t=0&f=0\"> link</a> for 'gaps' result<br>\n";
	}
}

main();
?>
