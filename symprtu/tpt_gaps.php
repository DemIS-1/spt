<?php
/* spt/list.php - list found prime tuples from database */
require_once("../inc/util.inc");
require_once("../inc/boinc_db.inc");

function show() {
  $t= get_int('t');
  $f= get_int('f');
  header("Content-type: text/plain");
  $db = BoincDb::get();
  $clausule=" k=0 ";
  if($t>0)
    $clausule=" k>5 ";
  $set = $db->do_query("select start, d, k, ofs, resid from spt_gap where $clausule order by start");
  echo "# Copyright boinc.termit.me Natalia Makarova & Alex Belyshev & Tomáš Brada, ask on forum about reuse or citation.\n";
  echo "# tp_gaps where $clausule\n";
  $ix=1;
  while($row = $set->fetch_object('stdClass')) {
    if($f==1) {
      echo "$ix {$row->start}\n";
    } else
    if($f==2) {
      echo "$ix ".($row->d+2)."\n";
    } else
    if($f==3) {
      echo "{$row->start}: 0 2";
      $a=2;
      foreach(explode(' ',$row->ofs) as $d) {
	if($d==="") continue;
	$a1=$a+$d;
	$a=$a1+2;
	echo " $a1 $a";
      }
      echo "\n";
      //echo "  [$ix {$row->resid}]\n";
    } else
    if($f==4) {
      echo "$ix todo A329164\n";
    } else
    if($f==5) {
      echo "$ix todo A329165\n";
    } else
    {
      echo "{$row->start}:{$row->ofs}  [$ix {$row->resid}]\n";
    }
    $ix++;
  }
  echo "# count = ".($ix-1)."\n";
  $set->free();
}

show();
