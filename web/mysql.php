<?

define("DB_ASSOC", MYSQL_ASSOC);
define("DB_NUM",   MYSQL_NUM);
define("DB_BOTH",  MYSQL_BOTH);

class MysqlDb {
	public $host;
	public $db;
	public $user;
	public $pass;
	public $persist;
	
	public $con;
	public $lasttime;
	public $queries;

	function __construct($host, $db, $user, $pass, $persist = false, $seqtable = null){

		$this->host = $host;
		$this->db = $db;
		$this->user = $user;
		$this->pass = $pass;
		$this->persist = $persist;
		$this->seqtable = $seqtable;
		$this->plogged = false;
		$this->preparedq = false;

		$this->con = null;
		$this->lasttime = 0;
		$this->qlog = "";

		$this->queries = array();
		$this->count = 0;
		$this->querytime = 0;
	}

	function __destruct(){
		$this->close();
	}
	
	function connect(){
		if($this->con){
			if($this->lasttime > time()-10)
				return mysql_ping($this->con);

			return true;
		}

		if(!$this->persist)
			$this->con = mysql_connect($this->host, $this->user, $this->pass);
		else
			$this->con = mysql_pconnect($this->host, $this->user, $this->pass);

		if(!$this->con)
			trigger_error("Database connect failed",E_USER_ERROR) && exit;

		if(!mysql_select_db($this->db))
			trigger_error("Can't select this database",E_USER_ERROR) && exit;

		return true;
	}

	function close(){
		if($this->con){
			mysql_close($this->con);
			$this->con = null;
		}
	}

	function query($query,$logthis = true){
		$insertid = 0;
		$affectedrows = 0;
		$numrows = 0;
		$countrows = 0;
		$qt = 0;
	
		$this->connect();

		$start = microtime(true);

		$result = mysql_query($query, $this->con);

		//if(substr($query, 0, 6) == "INSERT" || substr($query, 0, 6) == "UDPATE")
		$insertid = mysql_insert_id($this->con);
		//if(substr($query, 0, 6) == "UPDATE" || substr($query, 0, 6) == "DELETE" || substr($query, 0, 7) == "REPLACE" || substr($query, 0, 6) == "INSERT")
		$affectedrows = mysql_affected_rows($this->con);
		if(substr($query, 0, 6) == "SELECT" || substr($query, 0, 4) == "SHOW"){
			$numrows = mysql_num_rows($result);//this is mysql unique
			if(substr($query, 7, 19) == "SQL_CALC_FOUND_ROWS"){//this is mysql unique
				$res = mysql_query("SELECT FOUNDROWS()");
				$countrows = mysql_fetch_field($res, 0, 0);
			}
		}

		$end = microtime(true);

		if(!$result)
			trigger_error("Query Error: (" . mysql_errno($this->con) . ") " . mysql_error($this->con) . " : \"$query\"",E_USER_ERROR) && exit;

		$this->lasttime = time();

		$this->count++;
		$qt = $this->querytime = ($end - $start);
		
		if($logthis){
			if($this->plogged)
				$this->queries[] = array($this->qlog, $this->querytime);
			else
				$this->queries[] = array($query, $this->querytime);
		}
		if(count($this->queries) > 1000)
			array_shift($this->queries);

		return new MysqlDbResult($result, $this->con, $numrows, $affectedrows, $insertid, $countrows, $qt);
	}

	function prepare(){
		$this->connect();

		$args = func_get_args();
		
		if(count($args) == 0)
			trigger_error("mysql: Bad number of args (No args)",E_USER_ERROR) && exit;
		
		if(count($args) == 1)
			return $args[0];

		$query = array_shift($args);
		$parts = explode('?', $query);
		$query = array_shift($parts);
		
		if(count($parts) != count($args))
			trigger_error("Wrong number of args to prepare for $query",E_USER_ERROR) && exit;
		
		for($i = 0; $i < count($args); $i++){
			$query .= $this->prepare_part($args[$i]) . $parts[$i];
		}
		
		return $query;
	}

	function prepare_arr($query, $array){
		return call_user_func_array($query, $array);
	}

	function prepare_part($part){
		switch(gettype($part)){
			case 'integer': return $part;
			case 'double':  return $part;
			case 'string':  return "'" . mysql_real_escape_string($part, $this->con) . "'";
			case 'boolean': return ($part ? 1 : 0);
			case 'NULL':	return 'NULL';
			case 'array':
				$ret = array();
				foreach($part as $v)
					$ret[] = $this->prepare_part($v);
				return implode(',', $ret);
			default:
				trigger_error("Bad type passed to the database!!!!", E_USER_ERROR) && exit;
		}
	}

	function pquery(){
		$args = func_get_args();
		$this->preparedq = $args[0];
		$query = call_user_func_array(array($this, 'prepare'), $args);
		
		return $this->query($query);
	}
	
	function pquery_array($args){
		$this->preparedq = $args[0];
		$query = call_user_func_array(array($this, 'prepare'), $args);
		
		return $this->query($query);
	}

	function getSeqID($id1, $id2, $area, $table = false, $start = false){
		if(!$table){
			$table = $this->seqtable;
			if(!$table)
				return false;
		}
		$inid = $this->pquery("UPDATE " . $table . " SET max = LAST_INSERT_ID(max+1) WHERE id1 = ? && id2 = ? && area = ?", $id1, $id2, $area)->insertid();
		if($inid)
			return $inid;
			
		if(!$start)
			$start = 1;
			
		$ignore = $this->pquery("INSERT IGNORE INTO " . $table . " SET max = ?, id1 = ?, id2 = ?, area = ?", $start, $id1, $id2, $area);
		if($ignore->affectedrows())
			return $start;
		else	
			return $this->getSeqID($id1, $id2, $area, $table, $start);
	}
	
}

class MysqlDbResult {
	public $dbcon;
	public $result;
	public $numrows;
	public $affectedrows;
	public $insertid;
	public $countrows;
	public $querytime;

	function __construct($result, $dbcon, $numrows, $affectedrows, $insertid, $countrows,$qt){
		$this->dbcon = $dbcon;
		$this->result = $result;
		$this->numrows = $numrows;
		$this->affectedrows = $affectedrows;
		$this->insertid = $insertid;
		$this->countrows = $countrows;
		$this->querytime = $qt;
	}
	
	function __destruct(){
//		$this->free();
	}

	//one row at a time
	function fetchrow($type = DB_ASSOC){
		return mysql_fetch_array($this->result, $type);
	}

	//for queries with a single column in a single row
	function fetchfield(){
		$ret = $this->fetchrow(DB_NUM);
		return $ret[0];
	}

	//return the full set
	function fetchfieldset(){
		$ret = array();

		while($line = $this->fetchrow(DB_NUM)){
			if(count($line) == 1)
				$ret[] = $line[0];
			else
				$ret[$line[0]] = $line[1];
		}

		return $ret;
	}
	

	//return the full set
	function fetchrowset($col = null, $type = DB_ASSOC){
		$ret = array();

		while($line = $this->fetchrow($type))
			if($col)
				$ret[$line[$col]] = $line;
			else
				$ret[] = $line;

		return $ret;
	}

	function affectedrows(){
//		return mysql_affected_rows($this->dbcon);
		return $this->affectedrows;
	}

	function insertid(){
//		return mysql_insert_id($this->dbcon);
		return $this->insertid;
	}
	
	function rows(){
//		return mysql_num_rows($this->result);
		return $this->numrows;
	}

	function count_rows(){
		return $this->countrows;
	}

	function free(){
		return mysql_free_result($this->result);
	}
}

