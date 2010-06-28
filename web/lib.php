<?
include("mysql.php");
$db = new MysqlDb("localhost", "havannah", "havannah", "castro");


function make_select_list( $list, $sel = "" ){
	$str = "";
	foreach($list as $k => $v){
		if( $sel == $v )
			$str .= "<option value=\"" . htmlentities($v) . "\" selected> " . htmlentities($v) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($v) . "\"> " . htmlentities($v) . "</option>";
	}

	return $str;
}

function make_select_list_multiple( $list, $sel = array() ){
	$str = "";
	foreach($list as $k => $v){
		if(in_array($v, $sel))
			$str .= "<option value=\"" . htmlentities($v) . "\" selected> " . htmlentities($v) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($v) . "\"> " . htmlentities($v) . "</option>";
	}

	return $str;
}

function make_select_list_key( $list, $sel = null ){
	$str = "";
	foreach($list as $k => $v){
		if( $sel == $k )
			$str .= "<option value=\"" . htmlentities($k) . "\" selected> " . htmlentities($v) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($k) . "\"> " . htmlentities($v) . "</option>";
	}

	return $str;
}

function make_select_list_multiple_key( $list, $sel = array() ){
	$str = "";
	foreach($list as $k => $v){
		if(in_array($k, $sel))
			$str .= "<option value=\"" . htmlentities($k) . "\" selected> " . htmlentities($v) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($k) . "\"> " . htmlentities($v) . "</option>";
	}

	return $str;
}

function make_select_list_key_key( $list, $sel = "" ){
	$str = "";
	foreach($list as $k => $v){
		if( $sel == $v )
			$str .= "<option value=\"" . htmlentities($k) . "\" selected> " . htmlentities($k) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($k) . "\"> " . htmlentities($k) . "</option>";
	}

	return $str;
}

function make_select_list_col_key( $list, $col, $sel = "" ){
	$str = "";
	foreach($list as $k => $v){
		if( $sel == $k )
			$str .= "<option value=\"" . htmlentities($k) . "\" selected> " . htmlentities($v[$col]) . "</option>";
		else
			$str .= "<option value=\"" . htmlentities($k) . "\"> " . htmlentities($v[$col]) . "</option>";
	}

	return $str;
}

function make_radio($name, $list, $sel = "", $class = 'body'){
	$str = "";
	foreach($list as $k => $v){
		$str .= "<input type=radio name=\"$name\" value=\"" . htmlentities($v) . "\" id=\"$name/" . htmlentities($k) . "\"";
		if( $sel == $v )
			$str .= " checked";
		$str .= "><label for=\"$name/" . htmlentities($k) . "\" class=$class> " . htmlentities($v) . "</label> ";
	}

	return $str;
}

function make_radio_key($name, $list, $sel = "", $class = 'body' ){
	$str = "";
	foreach($list as $k => $v){
		$str .= "<input type=radio name=\"$name\" value=\"" . htmlentities($k) . "\" id=\"$name/" . htmlentities($k) . "\"";
		if( $sel == $k )
			$str .= " checked";
		$str .= "><label for=\"$name/" . htmlentities($k) . "\" class=$class> " . htmlentities($v) . "</label> ";
	}

	return $str;
}

function makeCatSelect($branch, $category = null){
	$str="";

	$prefix = array();

	foreach($branch as $cat){
		if(!isset($prefix[$cat['depth']]))
			$prefix[$cat['depth']] = str_repeat("- ", $cat['depth']);

		$str .= "<option value='$cat[id]'";
		if($cat['id'] == $category)
			$str .= " selected";
		$str .= ">" . $prefix[$cat['depth']] . $cat['name'] . "</option>";
	}

	return $str;
}

function makeCatSelect_multiple($branch, $category = array()){
	$str="";

	foreach($branch as $cat){
		$str .= "<option value='$cat[id]'";
		if(in_array($cat['id'], $category))
			$str .= " selected";
		$str .= ">" . str_repeat("- ", $cat['depth']) . $cat['name'] . "</option>";
	}

	return $str;
}

function makeCheckBox($name, $title, $checked = false){
	return "<input type=checkbox id=\"$name\" name=\"$name\"" . ($checked ? ' checked' : '') . "><label for=\"$name\"> $title</label>";
}

