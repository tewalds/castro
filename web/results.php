<?
include("lib.php");

function askresults($input){
	global $db;
	
	$baselines  = $db->query("SELECT baseline, CONCAT(name, ' (', params, ')') FROM baselines ORDER BY name")->fetchfieldset();
	$timelimits = $db->query("SELECT time,     CONCAT(name, ' (', params, ')') FROM times     ORDER BY time")->fetchfieldset();
	$players    = $db->query("SELECT player,   CONCAT(name, ' (', params, ')') FROM players   ORDER BY name")->fetchfieldset();
	$numgames   = $db->query("SELECT count(*) FROM games")->fetchfield();

?>
	<table><form action=/results method=GET>
		<tr>
			<td valign=top>
				Players:<br>
				<select name=players[] multiple=multiple size=15 style='width: 375px'><?= make_select_list_multiple_key($players, $input['players']) ?></select>
			</td>
			<td valign=top>
				Baselines:<br>
				<select name=baselines[] multiple=multiple style='width: 500px'><?= make_select_list_multiple_key($baselines, $input['baselines']) ?></select>
				<br><br>
				Time Limit:<br>
				<select name=times[] multiple=multiple style='width: 500px'><?= make_select_list_multiple_key($timelimits, $input['times']) ?></select>
				<br><br>
				<?= makeCheckBox("scale", "Scale Graph", $input['scale']) ?><br>
				<?= makeCheckBox("errorbars", "Show Errorbars", $input['errorbars']) ?><br>
				<?= makeCheckBox("simpledata", "Show Simple Data", $input['simpledata']) ?><br>
				<?= makeCheckBox("data", "Show Data", $input['data']) ?><br>
				<br>
				<input type=submit value="Show Graph!"> <?= $numgames ?> games logged
			</td>
		</tr>
	</form></table>


<?
	return true;
}

function showresults($input){
	global $db;

	askresults($input);

	if(count($input['baselines']) == 0 && count($input['players']) == 0 && count($input['times']) == 0)
		return true;

	if(count($input['baselines']) == 0 || count($input['players']) == 0 || count($input['times']) == 0){
		echo "You must select options from all categories to see any results!";
		return true;
	}

	$data = $db->pquery(
		"SELECT 
			results.player, 
			players.name, 
			sizes.params as size,
			SUM(wins) as wins,
			SUM(losses) as losses,
			SUM(ties) as ties,
			SUM(numgames) as numgames
		FROM results 
			JOIN players USING (player) 
			JOIN sizes USING (size)
		WHERE baseline IN (?) AND player IN (?) AND time IN (?)
		GROUP BY player, size
		ORDER BY name, results.size",
		$input['baselines'], $input['players'], $input['times'])->fetchrowset();


	$colors = array(
		"000000",
		"0000FF",
		"00FF00",
		"FF0000",
		"00FFFF",
		"FF00FF",
		"FFFF00",
		"0077FF",
		"00FF77",
		"7700FF",
		"FF0077",
		"77FF00",
		"FF7700"
	);

	$lbound = 50;
	$ubound = 50;
	$chd = array();
	$chm1 = array();
	$chm2 = array();
	$legend = array();
	foreach($data as $row){
		if(!isset($chd[$row['player']]))  $chd[$row['player']] = array(-1,-1,-1,-1,-1,-1,-1);
		if(!isset($chm1[$row['player']])) $chm1[$row['player']] = array(-1,-1,-1,-1,-1,-1,-1);
		if(!isset($chm2[$row['player']])) $chm2[$row['player']] = array(-1,-1,-1,-1,-1,-1,-1);

		$legend[$row['player']] = $row['name'];

		$rate = ($row['wins'] + $row['ties']/2.0)/$row['numgames'];
		$err = 2.0*sqrt($rate*(1-$rate)/$row['numgames']);

		$rate *= 1000;
		$err  *= 1000;

		if($lbound > $rate/10) $lbound = $rate/10;
		if($ubound < $rate/10) $ubound = $rate/10;

		$chd[$row['player']][$row['size']-4] = round($rate);
		$chm1[$row['player']][$row['size']-4] = max(round($rate - $err), 0);
		$chm2[$row['player']][$row['size']-4] = min(round($rate + $err), 1000);
	}

	if($input['scale']){
		$diff = $ubound - $lbound;
		$interval = ($diff <= 10 ? 1 : ($diff <= 20 ? 2 : ($diff <= 60 ? 5 : 10)));
		$lbound = max(0, floor($lbound/$interval)*$interval);
		$ubound = min(100, ceil($ubound/$interval)*$interval);
	}else{
		$interval = 10;
		$lbound = 0;
		$ubound = 100;
	}

#	ksort($chd);
#	ksort($chm1);
#	ksort($chm2);
#	ksort($legend);

	$num = count($chd);

	$chdlines = $chd;
	foreach($chdlines as & $line)  $line = implode(",", $line);

	$errorlines = "";
	if($input['errorbars'])
		foreach($chd as $k => $v)
			$errorlines .= "|" . implode(",", $chm1[$k]) . "|" . implode(",", $chm2[$k]);

	$chco = implode(",", array_slice($colors, 0, $num));

	if(count($input['players']) <= 9){
		echo "<table><tr><td>";
		echo "<img src=\"http://chart.apis.google.com/chart?" .
	//			"chs=750x400" . //size
				"chs=600x500" . //size
				"&cht=lc" . //line graph
				"&chxt=x,y". //,x,y" . //x and y axis
				"&chxr=0,4,10,1|1,$lbound,$ubound,$interval" . //4-10 on x, lbound-ubound on y
	//			"&chxl=2:|Board Size|3:|Win Rate" . //labels
	//			"&chxp=2,50|3,50" . //center the labels
				"&chco=$chco" . //line colours
	//			"&chdl=" . implode("|", $legend) . //legend
				"&chd=t$num:" . implode("|", $chdlines) . $errorlines . //data lines and errorlines
				"&chds=" . ($lbound*10) . "," . ($ubound*10) . //scale the data lines between upper and lower bound
				"&chm=h,FF0000,0," . (50 - $lbound)/($ubound - $lbound) . ",1"; //add 50% line
		if($input['errorbars']){
			$i = 0;
			foreach($chd as $v){
				echo "|E,$colors[$i]," . (2*$i + $num) . ",,1:7";
				$i++;
			}
		}

		//echo "&chof=validate";

		echo "\" />";
		echo "</td><td>";
		echo "<table>";
		$i = 0;
		foreach($legend as $n){
			echo "<tr><td bgcolor=$colors[$i]>&nbsp;&nbsp;&nbsp;&nbsp;</td><td>$n</td></tr>";
			$i++;
		}
		echo "</table>";
		echo "</td></tr></table>";
	}else{
		echo "A graph can't contain more than 9 players<br><br>";
	}


	if($input['simpledata']){
?>
		<br><br>
		<table width=800>
		<tr>
		<th colspan=2>Player</th>
		<th colspan=7>Board size</th>
		<th rowspan=2>Avg</th>
		</tr>
		<tr>
		<th>ID</th>
		<th>Name</th>
		<th>4</th>
		<th>5</th>
		<th>6</th>
		<th>7</th>
		<th>8</th>
		<th>9</th>
		<th>10</th>
		</tr>
<?
		$i = 1;
		foreach($chd as $p => $row){
			echo "<tr class='l" . ($i = 3 - $i) . "'>";
			echo "<td>$p</td>";
			echo "<td>$legend[$p]</td>";
			foreach($row as $s)
				echo "<td>" . number_format($s/10, 1) . "</td>";
			echo "<td>" . number_format(array_sum($row)/70, 1) . "</td>";
			echo "</tr>";
		}
		echo "</table>";
	}



	if($input['data']){
?>		<br><br>
		<table>
		<tr>
			<th colspan=2>Player</th>
			<th rowspan=2>Board<br>Size</th>
			<th colspan=2>Outcome</th>
			<th colspan=4>Games</th>
		</tr>
		<tr>
			<th>ID</th>
			<th>Name</th>
			<th>Win rate</th>
			<th>95% Conf</th>
			<th>Wins</th>
			<th>Losses</th>
			<th>Ties</th>
			<th>Total</th>
		</tr>
<?
		$i = 1;
		$name = "";
		foreach($data as $row){
			$rate = ($row['wins'] + $row['ties']/2.0)/$row['numgames'];
			$err = 2.0*sqrt($rate*(1-$rate)/$row['numgames']);
			echo "<tr class='l" . ($i = 3 - $i) . "'>";
			echo "<td>$row[player]</td>";
			echo "<td>" . ($row['name'] == $name ? '' : $row['name']) . "</td>";
			echo "<td align=center>$row[size]</td>";
			echo "<td align=right>" . number_format($rate*100, 1) . "</td>";
			echo "<td align=right>" . number_format($err*100, 1) . "</td>";
			echo "<td align=right>$row[wins]</td>";
			echo "<td align=right>$row[losses]</td>";
			echo "<td align=right>$row[ties]</td>";
			echo "<td align=right>$row[numgames]</td>";
			echo "</tr>";
			$name = $row['name'];
		}
		echo "</table>";
	}

	return true;
}


function gethosts($input){
	global $db;

	$data = $db->query("SELECT host, count(*) as count FROM games WHERE timestamp > UNIX_TIMESTAMP()-3600 GROUP BY host ORDER BY host")->fetchrowset();

	echo "<table>";
	echo "<tr>";
	echo "<th>Host</th>";
	echo "<th>Games</th>";
	echo "</tr>";
	$i = 1;
	$sum = 0;
	foreach($data as $row){
		echo "<tr class='l" . ($i = 3 - $i) . "'>";
		echo "<td>$row[host]</td>";
		echo "<td>$row[count]</td>";
		echo "</tr>";
		$sum += $row['count'];
	}
	echo "<tr class='f'>";
	echo "<td>" . count($data) . "</td>";
	echo "<td>$sum</td>";
	echo "</tr>";
	echo "</table>";

	return true;
}

function getrecent($input){
	global $db;

	$data = $db->query("SELECT players.name, count(*) as count FROM games, players WHERE games.player = players.player && timestamp > UNIX_TIMESTAMP()-3600 GROUP BY name ORDER BY name")->fetchrowset();
	echo "<table>";
	echo "<tr>";
	echo "<th>Player</th>";
	echo "<th>Games</th>";
	echo "</tr>";
	$i = 1;
	$sum = 0;
	foreach($data as $row){
		echo "<tr class='l" . ($i = 3 - $i) . "'>";
		echo "<td>$row[name]</td>";
		echo "<td>$row[count]</td>";
		echo "</tr>";
		$sum += $row['count'];
	}
	echo "<tr class='f'>";
	echo "<td>" . count($data) . "</td>";
	echo "<td>$sum</td>";
	echo "</tr>";
	echo "</table>";

	return true;
}


