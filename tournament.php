#!/usr/bin/php
<?
	error_reporting(E_ALL);

	$rounds = 5;
	$players = array(
		"./castro",
//		"./castro-rave-0",
		"./castro-rave-1-equal",
		"./castro-rave-1-not-equal",
	);
	
	$cmds = array(
		"boardsize 4", 
		"time_settings 0 1 0"
	);

//////////
	$num = count($players);
	$num_games = $num*($num-1)*$rounds;
	$time = time();

	echo "Starting a tournament of $rounds rounds and $num_games games\n\n";

	$results = array();
	for($i = 0; $i < $num; $i++){
		$results[$i] = array();
		for($j = 0; $j < $num; $j++)
			$results[$i][$j] = 0;
	}

	$g = 1;
	for($r = 0; $r < $rounds; $r++){
		for($i = 0; $i < $num; $i++){
			for($j = 0; $j < $num; $j++){
				if($i == $j)
					continue;

				echo "Game $g/$num_games: $players[$i] vs $players[$j]\n";
				$result = play_game($i, $j);
				echo "\n\n";

				if($result == 1)
					$results[$i][$j]++;
				else if($result == 2)
					$results[$j][$i]++;
				$g++;
			}
		}
	}
	
//////////////////////////
	echo "\n\n";
	foreach($players as $i => $player)
		echo "Player " . ($i+1) . ": $player\n";
	echo "\n";

//win vs loss matrix
	echo "Win vs Loss Matrix:\n";
	echo "   ";
	for($i = 0; $i < $num; $i++)
		printf(" %2d", $i+1);
	echo "\n";

	for($i = 0; $i < $num; $i++){
		$wins = 0;
		$losses = 0;
		printf("%2d:", $i+1);
		for($j = 0; $j < $num; $j++){
			if($i == $j){
				echo "   ";
			}else{
				printf(" %2d", $results[$i][$j]);
				$wins += $results[$i][$j];
				$losses += $results[$j][$i];
			}
		}
		echo " : $wins wins, $losses losses, " . (($num-1)*2*$rounds - $wins - $losses) . " ties\n";
	}
	echo "\n";

	$endtime = time();

	echo "Played $num_games games, Total Time: " . number_format($endtime - $time) . " s, Average Time: " . number_format(($endtime - $time)/$num_games) . " s\n";

///////////////////////////////////////

function play_game($p1, $p2){
	global $players, $cmds;

    $descriptorspec = array(
        0 => array("pipe", "r"),
        1 => array("pipe", "w"),
        2 => array("pipe", "w")
    );
	
	$fds = array();
	$pipes = array();
	
	$fds[0] = proc_open($players[$p1], $descriptorspec, $pipes[0]);
	$fds[1] = proc_open($players[$p2], $descriptorspec, $pipes[1]);
	
	if(!$fds[0] || !$fds[1])
		die("Failed to open the socket");

	foreach($cmds as $cmd){
		echo "$cmd\n";
		fwrite($pipes[0][0], "$cmd\n");
		fgets( $pipes[0][1]);
		fgets( $pipes[0][1]);

		fwrite($pipes[1][0], "$cmd\n");
		fgets( $pipes[1][1]);
		fgets( $pipes[1][1]);
	}

	$turnstrings = array("white","black");

	$turn = 0;
	while(1){
		echo "genmove $turnstrings[$turn]: ";
		fwrite($pipes[$turn][0], "genmove $turnstrings[$turn]\n");
		$ret = substr(fgets($pipes[$turn][1]), 2, -1);
		fgets($pipes[$turn][1]);
		echo "$ret\n";

		if($ret == "resign" || $ret == "none")
			break;

		fwrite($pipes[!$turn][0], "play $turnstrings[$turn] $ret\n");
		fgets($pipes[!$turn][1]);
		fgets($pipes[!$turn][1]);

		$turn = !$turn;
	}

	fwrite($pipes[0][0], "quit\n");
	fwrite($pipes[1][0], "quit\n");

	foreach($pipes as & $pipe)
		foreach($pipe as & $p)
			fclose($p);

	proc_close($fds[0]);
	proc_close($fds[1]);

	return !$turn +1;
}


