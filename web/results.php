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
				<select name=players[] multiple=multiple size=13 style='width: 375px'><?= make_select_list_multiple_key($players, $input['players']) ?></select>
			</td>
			<td valign=top>
				Baselines:<br>
				<select name=baselines[] multiple=multiple style='width: 500px'><?= make_select_list_multiple_key($baselines, $input['baselines']) ?></select>
				<br><br>
				Time Limit:<br>
				<select name=times[] multiple=multiple style='width: 500px'><?= make_select_list_multiple_key($timelimits, $input['times']) ?></select>
				<br><br>
				<?= makeCheckBox("errorbars", "Show Errorbars", $input['errorbars']) ?><br>
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

	if(count($input['players']) > 9){
		echo "You may not select more than 9 players";
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

		$chd[$row['player']][$row['size']-4] = (int)($rate*100);
		$chm1[$row['player']][$row['size']-4] = max((int)($rate*100 - $err*100), 0);
		$chm2[$row['player']][$row['size']-4] = min((int)($rate*100 + $err*100), 100);
	}

#	ksort($chd);
#	ksort($chm1);
#	ksort($chm2);
#	ksort($legend);

	$num = count($chd);

	foreach($chd as & $line)  $line = implode(",", $line);

	$errorlines = "";
	if($input['errorbars'])
		foreach($chd as $k => $v)
			$errorlines .= "|" . implode(",", $chm1[$k]) . "|" . implode(",", $chm2[$k]);


	$chco = implode(",", array_slice($colors, 0, $num));

	echo "<table><tr><td>";
	echo "<img src=\"http://chart.apis.google.com/chart?" .
//			"chs=750x400" . //size
			"chs=600x500" . //size
			"&cht=lc" . //line graph
			"&chxt=x,y". //,x,y" . //x and y axis
			"&chxr=0,4,10,1|1,0,100,10" . //4-10 on x, 0-100 on y
//			"&chxl=2:|Board Size|3:|Win Rate" . //labels
//			"&chxp=2,50|3,50" . //center the labels
			"&chco=$chco" . //line colours
//			"&chdl=" . implode("|", $legend) . //legend
			"&chd=t$num:" . implode("|", $chd) . $errorlines . //data lines and errorlines
			"&chm=h,FF0000,0,0.5,1";
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


	if($input['data']){
		echo "<br><br>";
		echo "<table>";
		echo "<tr bgcolor='#DDDDDD'>";
		echo "<td>Player ID</td>";
		echo "<td>Player Name</td>";
		echo "<td>Board size</td>";
		echo "<td>Win rate</td>";
		echo "<td>95% confidence</td>";
		echo "<td>Wins</td>";
		echo "<td>Losses</td>";
		echo "<td>Ties</td>";
		echo "<td>Games</td>";
		echo "</tr>";
		$colors = array('#F7F7F7', '#EFEFEF');
		$i = 0;
		foreach($data as $row){
			$rate = ($row['wins'] + $row['ties']/2.0)/$row['numgames'];
			$err = 2.0*sqrt($rate*(1-$rate)/$row['numgames']);
			echo "<tr bgcolor='$colors[$i]'>";
			echo "<td>$row[player]</td>";
			echo "<td>$row[name]</td>";
			echo "<td>$row[size]</td>";
			echo "<td>" . number_format($rate, 2) . "</td>";
			echo "<td>" . number_format($err, 2) . "</td>";
			echo "<td>$row[wins]</td>";
			echo "<td>$row[losses]</td>";
			echo "<td>$row[ties]</td>";
			echo "<td>$row[numgames]</td>";
			echo "</tr>";
			$i = 1-$i;
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
	echo "<td>Host</td>";
	echo "<td>Games</td>";
	echo "</tr>";
	$sum = 0;
	foreach($data as $row){
		echo "<tr>";
		echo "<td>$row[host]</td>";
		echo "<td>$row[count]</td>";
		echo "</tr>";
		$sum += $row['count'];
	}
	echo "<tr>";
	echo "<td>" . count($data) . "</td>";
	echo "<td>$sum</td>";
	echo "</tr>";
}

function getrecent($input){
	global $db;


}


