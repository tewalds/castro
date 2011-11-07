<?


$players = array(
	array('Knowledge Bonus Bridge, Connect ','-a 1 ',
		array(156,' 5,  5','-b 5 -c 5'),
		array(157,' 5, 10','-b 5 -c 10'),
		array(114,' 5, 20','-b 5 -c 20'),
		array(158,' 5, 50','-b 5 -c 50'),
		array(136,'10,  5','-b 10 -c 5'),
		array(135,'10, 10','-b 10 -c 10'),
		array(107,'10, 20','-b 10 -c 20'),
		array(137,'10, 50','-b 10 -c 50'),
		array(159,'25,  5','-b 25 -c 5'),
		array(160,'25, 10','-b 25 -c 10'),
		array(134,'25, 20','-b 25 -c 20'),
		array(161,'25, 50','-b 25 -c 25'),
		array(154,'50,  5','-b 50 -c 5'),
		array(153,'50, 10','-b 50 -c 10'),
		array(155,'50, 20','-b 50 -c 20'),
		array(162,'50, 50','-b 50 -c 50'),
	),
	array('Knowledge Bonus Bridge Reply ', '-a 1 -b ',
		array(77, '  1','1'),
		array(78, '  2.5','2.5'),
		array(56, '  5','5'),
		array(57, ' 10','10'),
		array(58, ' 25','25'),
		array(59, ' 50','50'),
		array(60, '100','100'),
		array(61, '250','250'),
		array(62, '500','500'),
	),
	array('Knowledge Bonus Connectivity ', '-a 1 -c ',
		array(79, ' 0.2','0.2'),
		array(80, ' 0.5','0.5'),
		array(68, ' 1','1'),
		array(64, ' 2','2'),
		array(69, ' 5','5'),
		array(70, '10','10'),
		array(81, '20','20'),
		array(82, '50','50'),
	),
	array('Knowledge Bonus Distance ', '-a 1 -D ',
		array(173,'  2','2'),
		array(174,'  5','5'),
		array(175,' 10','10'),
		array(176,' 20','20'),
		array(177,' 50','50'),
		array(178,'100','100'),
		array(179,'200','200'),
	),
	array('Knowledge Bonus Local Reply ', '-a 1 -l ',
		array(83, ' 0.2','0.2'),
		array(84, ' 0.5','0.5'),
		array(71, ' 1','1'),
		array(65, ' 2','2'),
		array(72, ' 5','5'),
		array(73, '10','10'),
		array(85, '20','20'),
		array(86, '50','50'),
	),
	array('Knowledge Bonus Locality ','-a 1 -y ',
		array(87, ' 0.2','0.2'),
		array(88, ' 0.5','0.5'),
		array(74, ' 1','1'),
		array(66, ' 2','2'),
		array(75, ' 5','5'),
		array(76, '10','10'),
		array(89, '20','20'),
		array(90, '50','50'),
	),
	array('Knowledge Extra FPU ', '',
		array(53, '0.7 All','-u 0.7 -l 1 -b 1 -c 1 -y 1'),
		array(52, '0.7 Bridge reply','-u 0.7 -b 1'),
		array(51, '0.7 Connectivity','-u 0.7 -c 1'),
		array(49, '0.7 Local Reply','-u 0.7 -l 1'),
		array(54, '0.7 Local Reply and Bridge Reply','-u 0.7 -l 1 -b 1'),
		array(50, '0.7 Locality','-u 0.7 -y 1'),
		array(46, '1.0 All','-u 1.0 -l 1 -b 1 -c 1 -y 1'),
		array(32, '1.0 Bridge reply','-u 1.0 -b 1'),
		array(31, '1.0 Connectivity','-u 1.0 -c 1'),
		array(29, '1.0 Local Reply','-u 1.0 -l 1'),
		array(45, '1.0 Local Reply and Bridge Reply','-u 1.0 -l 1 -b 1'),
		array(30, '1.0 Locality','-u 1.0 -y 1'),
	),
	array('Move Selection ', '',
		array(94, 'Avg UCT 0.2','-F 0 -E 0.2'),
		array(95, 'Avg UCT 0.5','-F 0 -E 0.5'),
		array(96, 'Avg UCT 1.0','-F 0 -E 1.0'),
		array(97, 'Avg UCT 2.0','-F 0 -E 2.0'),
		array(93, 'Num Simulations','-F -1'),
		array(104,'Num Wins','-F -2'),
		array(98, 'Rave    10','-F 10 -E 0'),
		array(99, 'Rave    50','-F 50 -E 0'),
		array(100,'Rave   500','-F 500 -E 0'),
		array(101,'Rave  5000','-F 5000 -E 0'),
		array(102,'UCT 1.0 Rave  50','-F 50 -E 1.0'),
		array(105,'UCT 1.0 Rave 100','-F 100 -E 1.0'),
		array(106,'UCT 1.0 Rave 200','-F 200 -E 1.0'),
		array(103,'UCT 1.0 Rave 500','-F 500 -E 1.0'),
	),
	array('Rave Decrease ', '',
		array(108,'   0 + n*   2','-f 0 -d 2'),
		array(109,'   0 + n*   5','-f 0 -d 5'),
		array(110,'   0 + n*  10','-f 0 -d 10'),
		array(111,'   0 + n*  20','-f 0 -d 20'),
		array(112,'   0 + n*  50','-f 0 -d 50'),
		array(113,'   0 + n* 100','-f 0 -d 100'),
		array(138,' 100 + n*   5','-f 100 -d 5'),
		array(139,' 100 + n*  10','-f 100 -d 10'),
		array(140,' 100 + n*  20','-f 100 -d 20'),
		array(141,' 100 + n*  50','-f 100 -d 50'),
		array(142,' 100 + n* 100','-f 100 -d 100'),
		array(143,' 100 + n* 200','-f 100 -d 200'),
		array(122,' 200 + n*   5','-f 200 -d 5'),
		array(123,' 200 + n*  10','-f 200 -d 10'),
		array(124,' 200 + n*  20','-f 200 -d 20'),
		array(125,' 200 + n*  50','-f 200 -d 50'),
		array(126,' 200 + n* 100','-f 200 -d 100'),
		array(127,' 200 + n* 200','-f 200 -d 200'),
		array(115,' 500 + n*   2','-f 500 -d 2'),
		array(116,' 500 + n*   5','-f 500 -d 5'),
		array(117,' 500 + n*  10','-f 500 -d 10'),
		array(118,' 500 + n*  20','-f 500 -d 20'),
		array(121,' 500 + n*  35','-f 500 -d 35'),
		array(119,' 500 + n*  50','-f 500 -d 50'),
		array(120,' 500 + n* 100','-f 500 -d 100'),
		array(144,' 500 + n* 200','-f 500 -d 200'),
		array(128,'1000 + n*   2','-f 1000 -d 2'),
		array(129,'1000 + n*   5','-f 1000 -d 5'),
		array(130,'1000 + n*  10','-f 1000 -d 10'),
		array(131,'1000 + n*  20','-f 1000 -d 20'),
		array(132,'1000 + n*  50','-f 1000 -d 50'),
		array(133,'1000 + n* 100','-f 1000 -d 100'),
		array(145,'1000 + n* 200','-f 1000 -d 200'),
	),
	array('Rave Explore ','',
		array(13, '0.1','-f 500 -e 0.1'),
		array(14, '0.2','-f 500 -e 0.2'),
		array(15, '0.5','-f 500 -e 0.5'),
		array(16, '1.0','-f 500 -e 1.0'),
	),
	array('Rave Extension ', '',
		array(18, 'Opponent Moves','-f 500 -o 1'),
		array(17, 'Scale','-f 500 -r 1'),
		array(20, 'Short Rollouts','-f 500 -s 1'),
		array(19, 'Skip Rave','-f 500 -i 20'),
	),
	array('Rave Only ', '-e 0 -d 0 -f ',
		array(1,  '   20','20'),
		array(2,  '   50','50'),
		array(3,  '  100','100'),
		array(4,  '  200','200'),
		array(5,  '  500','500'),
		array(6,  ' 1000','1000'),
		array(38, ' 1500','1500'),
		array(39, ' 2000','2000'),
		array(48, ' 5000','5000'),
		array(92, '10000','10000'),
	),
	array('Rollout Policy Check Rings Prob', '',
		array(180,'0.05','-C 0.05'),
		array(181,'0.10','-C 0.10'),
		array(182,'0.20','-C 0.20'),
		array(183,'0.30','-C 0.30'),
		array(184,'0.50','-C 0.50'),
		array(185,'0.80','-C 0.80'),
	),
	array('Rollout Policy Check Rings Depth ', '-R ',
		array(186,'  5','5'),
		array(187,' 10','10'),
		array(188,' 15','15'),
		array(189,' 20','20'),
		array(191,' 30','30'),
		array(192,' 40','40'),
		array(190,' 50','50'),
		array(193,' 60','60'),
		array(194,' 70','70'),
		array(195,' 80','80'),
		array(196,' 90','90'),
		array(197,'100','100'),
		array(198,'110','110'),
		array(199,'120','120'),
		array(200,'130','130'),
		array(214,'140','140'),
		array(215,'150','150'),
		array(216,'160','160'),
		array(217,'170','170'),
	),
	array('Rollout Policy Check Rings Depth Avg ', '-R ',
		array(201,'-0.5','-0.50'),
		array(202,'-1.0','-1'),
		array(203,'-1.5','-1.5'),
		array(204,'-2.0','-2'),
		array(205,'-2.5','-2.5'),
	),
	array('Rollout Policy Check Rings Depth Empty ', '-R ',
		array(206,'-0.2','-0.2'),
		array(207,'-0.3','-0.3'),
		array(208,'-0.4','-0.4'),
		array(209,'-0.5','-0.5'),
		array(210,'-0.6','-0.6'),
		array(211,'-0.7','-0.7'),
		array(212,'-0.8','-0.8'),
		array(213,'-0.9','-0.9'),
	),
	array('Rollout Policy Detect Win ', '-w 1 -W ',
		array(163,'   2',   '2'),
		array(164,'   5',   '5'),
		array(165,' 10',    '10'),
		array(166,' size',  '-1'),
		array(167,' size*2','-2'),
		array(36, '1000',   '1000'),
	),
	array('Rollout Policy Detect Win and Loss ', '-w 2 -W ',
		array(168,'   2',  '2'),
		array(169,'   5',  '5'),
		array(170,'  10',  '10'),
		array(37, '1000',  '1000'),
		array(171,'size',  '-1'),
		array(172,'size*2','-2'),
	),
	array('Rollout Policy ', '',
		array(35, 'Bridge and Last Good Reply','-p 1 -g 1'),
		array(33, 'Bridge Reply','-p 1'),
		array(34, 'Last Good Reply wins','-g 1'),
		array(91, 'Last Good Reply wins+losses','-g 2'),
		array(55, 'Weighted Random','-h 1'),
	),
	array('Tree Growth Backups ', '-m ',
		array(22, 'Disabled','0'),
		array(23, 'One ply', '1'),
	),
	array('Tree Growth Keep Tree ', '',
		array(21, 'Disable','-k 0'),
	),
	array('Tree Growth Expand Experience ', '-x ',
		array(27, '2','2'),
		array(40, '3','3'),
		array(28, '4','4'),
		array(41, '5','5'),
		array(47, '6','6'),
	),
	array('Tree Growth First play urgency ', '-u ',
		array(42, '0.5','0.5'),
		array(43, '0.6','0.6'),
		array(24, '0.7','0.7'),
		array(25, '0.8','0.8'),
		array(26, '0.9','0.9'),
	),
	array('Tree Multiple Rollout ', '-O ',
		array(218,' 2','2'),
		array(219,' 3','3'),
		array(220,' 4','4'),
		array(221,' 5','5'),
		array(222,' 6','6'),
		array(223,' 7','7'),
		array(224,' 8','8'),
		array(225,' 9','9'),
		array(226,'10','10'),
		array(227,'12','12'),
		array(228,'15','15'),
		array(229,'20','20'),
	),
	array('UCT Only ', '-f 0 -e ',
		array(44, '0.5','0.5'),
		array(7,  '0.6','0.6'),
		array(8,  '0.7','0.7'),
		array(9,  '0.8','0.8'),
		array(10, '0.9','0.9'),
		array(11, '1.0','1.0'),
		array(12, '1.1','1.1')
	),
);

/*
// havannah.baselines
$baselines = array(
  array('baseline'=>1,'weight'=>0,'name'=>'Basic UCT only','params'=>'-t 1 -o 0 -E 1 -F -1 -e 0.9 -f 0 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0'),
  array('baseline'=>2,'weight'=>0,'name'=>'Basic Rave only','params'=>'-t 1 -o 0 -E 1 -F -1 -e 0 -f 500 -d 0 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0'),
  array('baseline'=>3,'weight'=>4,'name'=>'Rave Decrease','params'=>'-t 1 -o 0 -E 0.5 -F -1 -e 0 -f 500 -d 200 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0'),
  array('baseline'=>4,'weight'=>4,'name'=>'Rave Decrease know','params'=>'-t 1 -o 0 -E 0 -F -2 -e 0 -f 500 -d 200 -k 1 -m 2 -u 1 -x 1 -r 1 -X 1 -s 0 -l 0 -y 0 -c 20 -b 25 -p 0 -h 0 -g 0 -w 0 -W 0')
);

// havannah.times
$times = array(
  array('time'=>1,'weight'=>0,'name'=>'100 seconds per side per game','params'=>'100 0 0'),
  array('time'=>2,'weight'=>7,'name'=>' 2 seconds per move','params'=>'0 2 0'),
  array('time'=>3,'weight'=>8,'name'=>' 5 seconds per move','params'=>'0 5 0'),
  array('time'=>4,'weight'=>10,'name'=>' 1 second per move','params'=>'0 1 0'),
  array('time'=>5,'weight'=>5,'name'=>'10 seconds per move','params'=>'0 10 0')
);
*/



include("mysql.php");
$olddb = new MysqlDb("127.0.0.1", "havannah", "havannah", "castro");
$newdb = new MysqlDb("127.0.0.1", "paramlog", "paramlog", "castro");


$baselinemap = array(
	1 => 2,
	2 => 3,
	3 => 4,
	4 => 5,
);

$newdb->query("TRUNCATE players");

$newdb->query("INSERT INTO `players` (`id`, `userid`, `parent`, `type`, `weight`, `name`, `params`, `comment`) VALUES
(1, 1, 0, 2, 1, 'Castro', 'Castro', ''),
(2, 1, 1, 3, 1, 'Basic UCT Only', '-t 1 -o 0 -E 1 -F -1 -e 0.9 -f 0 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0', ''),
(3, 1, 1, 3, 1, 'Basic Rave Only', '-t 1 -o 0 -E 1 -F -1 -e 0 -f 500 -d 0 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0', ''),
(4, 1, 1, 3, 1, 'Rave Decrease', '-t 1 -o 0 -E 0.5 -F -1 -e 0 -f 500 -d 200 -k 1 -m 2 -u 1 -x 1 -i 0 -s 0 -l 0 -y 0 -c 0 -b 0 -p 0 -h 0 -g 0 -w 0', ''),
(5, 1, 1, 3, 1, 'Rave Decrease Knowledge', '-t 1 -o 0 -E 0 -F -2 -e 0 -f 500 -d 200 -k 1 -m 2 -u 1 -x 1 -r 1 -X 1 -s 0 -l 0 -y 0 -c 20 -b 25 -p 0 -h 0 -g 0 -w 0 -W 0', '')"
);

$playermap = array();

foreach($players as $group){
	$gname = array_shift($group);
	$gparam = array_shift($group);

	$gid = $newdb->pquery("INSERT INTO players SET userid = 1, type = 4, parent = 1, name = ?, params = ?, weight = 0", $gname, $gparam)->insertid();

	foreach($group as $p){
		$pid = $newdb->pquery("INSERT INTO players SET userid = 1, type = 5, parent = ?, name = ?, params = ?, weight = 0", $gid, $p[1], $p[2])->insertid();
		$playermap[$p[0]] = $pid;
	}
}


$newdb->query("TRUNCATE games");
$newdb->query("TRUNCATE moves");
$newdb->query("TRUNCATE results");


$res = $olddb->query("SELECT * FROM games", false);

while($line = $res->fetchrow())
	if(isset($playermap[$line['player']]))
		save_game(parserow($line), 1);

/////////////////////////////

function hgf2ga($in, $size){
	if($in == "swap")
		return $in;

	$y = ord(strtolower($in[0])) - ord('a');
	$x = (int)substr($in, 1)-1;

	if($size && $y >= $size)
		$x += $y+1 - $size;

	return chr(ord('a') + $y) . ($x+1);
}

#echo HGF2GA('D4', 4) . " - D4\n";
#echo HGF2GA('B2', 4) . " - B2\n";
#echo HGF2GA('F3', 4) . " - F5\n";

function parserow($input){
	global $playermap, $baselinemap;

	$output = array(
		"lookup"  => "",
		"player1" => $baselinemap[$input['baseline']],
		"player2" => $playermap[$input['player']],
		"size"    => $input['size'],
		"time"    => $input['time'],
		"outcome1" => $input['outcome'],
		"outcome2" => $input['outcome'],
		"outcomeref" => $input['outcome'],
		"version1" => '',
		"version2" => '',
		"timestamp" => $input['timestamp'],
		"host" => $input['host'],
		"saveresult" => true,
	);


	$lines = explode("\n", $input['log']);
	while(count($lines) && substr($lines[0], 0, 5) != "play "){
		$line = explode(" ", array_shift($lines));

		if($line[0] == "boardsize")
			$size = $line[1];
	}

	$moves = array();
	$i = 1;
	while(count($lines)){
		$play = explode(" ", array_shift($lines));
		$time = explode(" ", array_shift($lines));

	//	echo "play "; print_r($play);
	//	echo "time "; print_r($time);

		if($play[0] == 'play'){
			$moves[] = array("movenum" => $i, "position" => hgf2ga($play[2], $size), "side" => 0, "value" => 0, "outcome" => 0,
							"timetaken" => $time[2], "work" => 0, "nodes" => 0, "comment" => "");
		}else{
			$winner = $play[2];
			$totaltime = $time[3];
			break;
		}
		array_shift($lines);
		$i++;
	}
	$nummoves = count($moves);

	$first = ( $input['outcome'] == (2-($nummoves%2)) ? 1 : 2);

	$i = $first;
	foreach($moves as & $move){
		$move['side'] = $i;
		$i = 3-$i;
	}

	$output['moves'] = $moves;

	return $output;
}


function save_game($data, $userid){
	global $newdb;

	$outcome = ($data['outcomeref'] ? $data['outcomeref'] : ($data['outcome1'] == $data['outcome2'] ? $data['outcome1'] : 0));


	$data['userid'] = $userid;
	$data['id'] = $newdb->pquery("INSERT INTO games SET userid = ?, lookup = ?, player1 = ?, player2 = ?, size = ?, time = ?,
		timestamp = ?, nummoves = ?, outcome1 = ?, outcome2 = ?, outcomeref = ?, version1 = ?, version2 = ?, host = ?, saved = ?",
		$userid, $data['lookup'], $data['player1'], $data['player2'], $data['size'], $data['time'], time(), count($data['moves']),
		$data['outcome1'], $data['outcome2'], $data['outcomeref'], $data['version1'], $data['version2'], $data['host'], 1)->insertid();


	$query = "INSERT INTO moves (userid, gameid, movenum, position, side, value, outcome, timetaken, work, nodes, comment) VALUES ";

	$parts = array();
	foreach($data['moves'] as $move)
		$parts[] = $newdb->prepare("(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
			$userid, $data['id'], $move['movenum'], $move['position'], $move['side'], $move['value'], $move['outcome'], $move['timetaken'], $move['work'], $move['nodes'], $move['comment']);

	if(count($data['moves']))
		$newdb->query($query . implode(", ", $parts));

#	foreach($data['moves'] as $move)
#		$newdb->pquery("INSERT INTO moves SET userid = ?, gameid = ?, movenum = ?, position = ?, side = ?,	value = ?, outcome = ?, timetaken = ?, work = ?, nodes = ?, comment = ?",
#			$userid, $data['id'], $move['movenum'], $move['position'], $move['side'], $move['value'], $move['outcome'], $move['timetaken'], $move['work'], $move['nodes'], $move['comment']);


	$newdb->pquery("INSERT INTO results SET userid = ?, player1 = ?, player2 = ?, size = ?, time = ?, weight = 1, p1wins = ?, p2wins = ?, ties = ?, numgames = 1
		ON DUPLICATE KEY UPDATE p1wins = p1wins + ?, p2wins = p2wins + ?, ties = ties + ?, numgames = numgames + 1",
		$userid, $data['player1'], $data['player2'], $data['size'], $data['time'],
		(int)($outcome == 1), (int)($outcome == 2), (int)($outcome == 3),
		(int)($outcome == 1), (int)($outcome == 2), (int)($outcome == 3));
}

