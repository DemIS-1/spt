<?php
/* spt/explore.php - prime tuple explorer */
/* spt_explore.php - is copy of previos for repair functionality */

require_once("../inc/util.inc");
require_once("../inc/boinc_db.inc");

//ini_set('max_execution_time', 120);
//set_time_limit(240);
$global_limit_query_records=200000;

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
	$a[0]=0;
	$astr=implode(' ',$a);
	return $astr;
}

function get_diameter($f_tuple) {
	$typle_arr=explode(' ',$f_tuple);
	return max($typle_arr);
}

function spt_acronym_show(){
	echo "<hr>Where: ";
	echo "SPT - Symmetric Prime Tuples<br>\n";
	echo "TPT - Twin Prime Tuples<br>\n";
	echo "STPT - Symmetric Twin Prime Tuples<br>\n";
}

function spt_search_form_show($f_search) {
	if($f_search=="") {
    echo "
	<div><form>
        <input type='text' name='near' value='5179852391836338889'/>
        <input type='submit' value='find'/>
        </form></div>
    ";
	}else{
		echo "
        <div><form>
        <input type='text' name='near' value='$f_search'/>
        <input type='submit' value='find'/>
        </form></div>
    ";

	}
}

function spt_big_number_oper($big_num1,$big_num2,$big_oper) {
	$db = BoincDb::get();
	$set = $db->do_query("select @sum:=$big_num1 $big_oper $big_num1 as summ;");
	$row = $set->fetch_object('stdClass');
	$sumR=$row->summ;
	return $sumR;
}

function spt_near_show($f_find) {
	start_table(); // need table class="table table-condensed" if set 'class="table table-conden>
	echo "<tbody>";

	$db = BoincDb::get();
	$query="SELECT `start`, id, k, ofs, kind
		 FROM spt
		 WHERE (spt.`start` >=$f_find - 500000000 and spt.`start`<=$f_find + 500000000)
		 OR (spt.`start` = $f_find)
		 ORDER BY spt.`start` limit 12
		 ;"; //danger!!! need unescape function!!!
	$set = $db->do_query($query);
	while($row = $set->fetch_object('stdClass')) {
		$astr=convert_twin_tuple($row->start,$row->k);
		$k_up=strtoupper($row->kind);
		$spt_calc = gmp_sub($row->start,$f_find);
		echo "<tr><td>$k_up($row->k)</td><td><a href='".$_SERVER['PHP_SELF']."?$row->kind=$row->k&s=$row->start'>{$row->start}</a></td><td>$astr</td><td>$spt_calc</td></tr>\n";
	}

	echo "</tbody>";
	end_table();
	$set->free();
}

function spt_list_show() {
echo "
	<div><ul>
		<li><a href='?rare'>Show rare tuples</a></li>
		<li><a href='?count'>Count tuples by kind</a>(todo)</li>
		<li><a href='tpt_gaps.php?t=0&amp;f=0'>Record gaps in twin prime tuples</a></li>
		<li><a href='tpt_gaps.php?t=6&amp;f=0'>Record gaps in twin prime tuples, k>=6</a></li>
	</ul>
	";
}

function spt_export_show() {
	echo "<h3>Exporting tuples</h3>\n";
	echo "
	<div>
	<p>Use <a href='tuples.php?spt=20&amp;p=1&amp;ln'>tuples.php?[kind]=[k]&amp;p=[page]</a>(todo 1)<br>
	(show like git <a href='spt_list.php?batch=91'>spt_list.php</a> (todo 2 correct) )<br>
	(or like <a href='spt_list.php?k=20&p=1'>spt_list.php</a> (todo 3 correct) ) , where</p>
	<ul>
		<li>kind is spt, stpt or tpt</li>
		<li>k is the order of the tuple and number of primes in the tuple</li>
		<li><del>k used to be number of pairs, which was confusing</del></li>
		<li>page is integer 0, 1, 2... </li>
		<li>add &amp;cp for compact view</li>
		<li>add &amp;ln for clickable html output</li>
	</ul>
	<p>Stable batch: (todo)<br>
	Batch list in work: counter(todo)<br>
	Batch list by user: counter(todo ???)<br>
	(need make caching result)</p>
	</div>
	";
}

function spt_count_show() {
	$db = BoincDb::get();
	global $global_limit_query_records;
	$query="SELECT spt.kind as Kind,spt.k,COUNT(*) as Count
		 FROM spt
		 GROUP BY spt.kind,spt.k
		 ORDER BY Count desc
		 ;";
	$set = $db->do_query($query);

	start_table();
	echo "<thead>";
	echo "<tr><td>Kind(k)</td><td>Count</td><td>pages</td><td>last batch</td></tr>";
	echo "</thead>";
	echo "<tbody>";
	while($row = $set->fetch_object('stdClass')) {
		$astr=0;
		if(($row->Kind == "spt") && ($row->k == 10)) {
			$a=0;
		}else{
			$k_up=strtoupper($row->Kind);
			echo "<tr><td><a href=\"tuples.php?$row->Kind=$row->k&p=1&ln\" >$k_up($row->k)</a></td><td>$row->Count</td><td>" . ceil($row->Count / $global_limit_query_records) ."</td><td>" . 0 . " (todo)</td></tr>\n";
		}
	}
	echo "</tbody>";
	end_table();
	$set->free();
}

function spt_rare_show($f_mode_par,$f_type_par,$f_par) {
	$db = BoincDb::get();
	if($f_par=="") {
		$query="SELECT distinct(start), k, ofs, kind
			 FROM spt
			 WHERE start >= 0 and start <= 9000000000000000000
			 AND (( kind='spt' and mod(k,2)=0 and k>=24 ) or ( kind='spt' and mod(k,2)=1 and k>=17 ) or ( kind='stpt' and k>=16 ) or ( kind='tpt' and k>=20 ))
			 ORDER BY k, start
			 ;";
		$set = $db->do_query($query);
		echo "<p><code>start >= 0 and start <= 9000000000000000000 and (( kind='spt' and mod(k,2)=0 and k>=24 ) or ( kind='spt' and mod(k,2)=1 and k>=17 ) or ( kind='stpt' and k>=16 ) or ( kind='tpt' and k>=20 ))</code></p>\n";
	}else{
echo "(todo: need make correct select from sql)<br>\n";
		global $global_limit_query_records;
		$query="SELECT distinct(start), k, ofs, kind
			 FROM spt
			 WHERE (start >= $f_par - 900000000000 and start <= $f_par + 900000000000) or (start = $f_par)
			 ORDER BY start limit 12
			 ;";
		$set = $db->do_query($query);
	}
	start_table();
	echo "<tbody>";
	while($row = $set->fetch_object('stdClass')) {
		$astr=convert_twin_tuple($row->start,$row->k);
		$k_up=strtoupper($row->kind);
		if($f_par=="") {
			echo "<tr><td>$k_up($row->k)</td><td><a href='".$_SERVER['PHP_SELF']."?$row->kind=$row->k&s=$row->start'>{$row->start}</a></td><td>$astr</td></tr>\n";
		}else{
			$spt_calc = gmp_sub($row->start,$f_par);
			if($spt_calc != 0) {
				echo "<tr><td>$k_up($row->k)</td><td><a href='".$_SERVER['PHP_SELF']."?$row->kind=$row->k&s=$row->start'>{$row->start}</a></td><td>$astr</td><td>$spt_calc</td></tr>\n";
			}
		}
	}
	echo "</tbody>";
	end_table();
	$set->free();
}

function spt_rare_top_show($f_mode_par,$f_type_par,$f_par) {
	$db = BoincDb::get();
	$global_limit_query_records=1;
	$query="SELECT distinct(start), k, ofs, kind, userid, resid, batch
		 FROM spt
		 WHERE start=$f_par
		 AND k=$f_type_par
		 AND kind='$f_mode_par'
		 LIMIT 0,$global_limit_query_records
		 ;";
	$set = $db->do_query($query);
	$row = $set->fetch_object('stdClass');
	if($row) {
		$astr_k2= 0;
		if(($row->kind == "tpt")||($row->kind == "stpt")) {
			$astr_k2= $row->k / 2;
		}
		$astr=convert_twin_tuple($row->start,$row->k);
		$spt_ofs=$row->ofs;
		$spt_user_id=$row->userid;
		$spt_type=$row->kind;
		$spt_batch=$row->batch;
		$spt_res_id=$row->resid;
		$spt_k=$row->k;
		$k_up=strtoupper($spt_type);
		$prime_msg="<div>Prime p<sub>0</sub>= <b>$f_par</b> starts ";
		if($spt_type=="tpt"){
			$prime_msg=$prime_msg . "of tuple ";
			$prime_msg=$prime_msg . "of <b>$f_type_par</b> consecutive twin primes ";
			$prime_msg=$prime_msg . ", which form <b>$astr_k2</b> ";
		}else if($spt_type=="stpt"){
			$prime_msg=$prime_msg . "of symmetrical tuple ";
			$prime_msg=$prime_msg . "of <b>$f_type_par</b> consecutive twin primes ";
			$prime_msg=$prime_msg . ", which form <b>$astr_k2</b>  ";
		}else if($spt_type=="spt"){
			$prime_msg=$prime_msg . "of symmetrical tuple ";
			$prime_msg=$prime_msg . "of <b>$f_type_par</b> consecutive primes ";
		}
		$prime_msg=$prime_msg . "($k_up).<br>\n";
		echo "$prime_msg";
		echo "d<sub>n</sub>: $spt_ofs (???)<br/>\n";
		echo "a<sub>n</sub>: $astr<br/>\n";
		echo "All $f_type_par integers p<sub>n</sub> = p<sub>0</sub>+a<sub>n</sub> = p<sub>n-1</sub>+d<sub>n</sub> are prime and there are no other primes in the gaps.<br/>\n";
		$t_diam = get_diameter($astr);
		echo "diameter: $t_diam<br/>\n";
		$set = $db->do_query("select name from user where id = $spt_user_id"); //dan>
		$row = $set->fetch_object('stdClass');
		$spt_user_name=$row->name;
		echo "Found by: <a href='/adsl/show_user.php?userid=$spt_user_id'>$spt_user_name</a><br/>\n";
		echo "db: batch=$spt_batch, k=$spt_k, $spt_ofs<br/>\n";
		echo "</div>";
	}else{
		echo "No such data for $f_par.<br>\n";
	}
	$set->free();
}

function main() {
	page_head("Prime Tuple Database");
	if(null!==$f_batch=get_str('rare',true)) {
		spt_rare_show("","",""); //run rare from this point !
	}else if(null!==$f_batch=get_str('count',true)){
		spt_count_show(); //run counter from this point ! and before spt_search_form_show()
		spt_search_form_show("");
		spt_list_show();
		spt_export_show();
	}else if(((null!==$f_type=get_int('spt',true))||(null!==$f_type=get_int('stpt',true))||(null!==$f_type=get_int('tpt',true)) )&&(null!==$f_num=get_int('s',true))){
		if(null!==$f_mode=get_str('spt',true)){
			$f_mode="spt";
		}else if(null!==$f_mode=get_str('stpt',true)){
			$f_mode="stpt";
		}else if(null!==$f_mode=get_str('tpt',true)){
			$f_mode="tpt";
		}
		spt_rare_top_show($f_mode,$f_type,$f_num);
		spt_rare_show($f_mode,$f_type,$f_num);
		spt_acronym_show();
	}else if(null!==$f_near=get_int('near',true)){
		spt_search_form_show($f_near);
		spt_near_show($f_near);
	}else{
		echo "<p>".tra("Find primes tuples nearby")."</p>\n";
		spt_search_form_show("");
		spt_list_show();
		spt_export_show();
	}
	page_tail();
}

main();

?>
