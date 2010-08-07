<?
include("lib.php");


function players_route($input){
	switch($input['action']){
		case 'disable': return players_disable($input);
		case 'edit'   : return players_edit($input);
		default:        return players_list($input);
	}
}

function players_list($input){
	global $db;
	
	$players = $db->query("SELECT * FROM players ORDER BY name")->fetchrowset();

?>

	<form action="/players/route" method="post">
	<table>
	<tr>
		<th></td>
		<th>ID</td>
		<th>Name</td>
		<th>Weight</td>
		<th>Param</td>
	</tr>
<?	$i = 1;
	foreach($players as $player){ ?>
		<tr class="l<?= ($i = 3 - $i) ?>">
		<td><input type=checkbox name=check[] value="<?= $player['player'] ?>"></td>
		<td><?= $player['player'] ?></td>
		<td><?= $player['name']   ?></td>
		<td><?= $player['weight'] ?></td>
		<td><?= $player['params'] ?></td>
		</tr>
<?	} ?>
	<tr class='f'>
		<td colspan='5'>
			<input type=submit name=action value=disable>
			<input type=submit name=action value=edit>
		</td>
	</tr>
	</table>
	</form>

<br><br>

	<form action="/players/add" method="post">
	<table id="addplayers">
		<tr><th colspan="3">Add Players</th></tr>
		<tr><th>Name</th><th>Weight</th><th>Params</th></tr>
		<tr>
			<td><input type="text" name="names[]"   value="" size=40 /></td>
			<td><input type="text" name="weights[]" value="" size=5  /></td>
			<td><input type="text" name="params[]"  value="" size=30 /></td>
		</tr><tr id="before" class='f'>
			<td colspan="3">
				<input type="submit" value="Add Players" /> <a class="body" href="javascript: copyInputRow('addplayers','before'); void(0);">Add a row</a>. Blank rows will be ignored.
			</td>
		</tr>
	</table>
	</form>
	
<?
	return true;
}


function players_disable($input){
	global $db;

	$db->pquery("UPDATE players SET weight = 0 WHERE player IN (?)", $input['check']);
	
	echo "Players disabled<br>";
	
	return players_list($input);
}

function players_edit($input){
	global $db;

	$players = $db->pquery("SELECT * FROM players WHERE player IN (?) ORDER BY name", $input['check'])->fetchrowset();

?>
	<form action="/players/update" method="post">
	<table>
	<tr><th colspan=4>Edit players</th></tr>
	<tr>
		<th>ID</th>
		<th>Name</th>
		<th>Weight</th>
		<th>Param</th>
	</tr>
<?
	$i = 1;
	foreach($players as $player){
?>		<tr class="l<?= ($i = 3 - $i) ?>">
		<td><input type=hidden name="players[]" value="<?= $player['player'] ?>"/><?= $player['player'] ?></td>
		<td><input type="text" name="names[]"   value="<?= htmlentities($player['name'])   ?>" size=40 /></td>
		<td><input type="text" name="weights[]" value="<?= htmlentities($player['weight']) ?>" size=5  /></td>
		<td><input type="text" name="params[]"  value="<?= htmlentities($player['params']) ?>" size=30 /></td>
		</tr>
<?	} ?>
	<tr class='f'><td colspan=4>
		<input type=submit name=action value=Update>
	</td></tr>
	</table>
	</form>
<?
	return true;
}

function players_update($input){
	global $db;

	for($i = 0; $i < count($input['players']); $i++)
		$db->pquery("UPDATE players SET name = ?, weight = ?, params = ? WHERE player = ?", 
			$input['names'][$i], $input['weights'][$i], $input['params'][$i], $input['players'][$i]);

	echo "Players Updated<br>\n";
	
	return players_list(array());
}

function players_add($input){
	global $db;

	for($i = 0; $i < count($input['names']); $i++){
		if(empty($input['names'][$i]) || !isset($input['weights'][$i]) || empty($input['params'][$i]))
			continue;
	
		if($db->pquery("INSERT INTO players SET name = ?, weight = ?, params = ?",
			$input['names'][$i], $input['weights'][$i], $input['params'][$i])->affectedrows() == 1)
			echo $input['names'][$i] . " input successfully<br>\n";
		else
			echo $input['names'][$i] . " failed<br>\n";
	}
	return players_list(array());
}

