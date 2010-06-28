<?
include("lib.php");

function getwork($data){
	global $db;
	
	$res = $db->query(
		"SELECT
			baselines.baseline AS b,
			baselines.params AS bp,
			players.player AS p,
			players.params AS pp,
			sizes.size AS s,
			sizes.params AS sp,
			times.time AS t,
			times.params AS tp
		FROM 
			baselines
			JOIN players
			JOIN sizes
			JOIN times 
			LEFT JOIN results USING (baseline, player, size, time)
		WHERE
			baselines.weight > 0 AND
			players.weight > 0 AND
			sizes.weight > 0 AND
			times.weight > 0 AND
			(results.weight IS NULL OR results.weight > 0)
		ORDER BY
			(results.numgames / (baselines.weight * players.weight * sizes.weight * times.weight * IF(results.weight IS NULL, 1, results.weight))) ASC, 
			RAND() 
		LIMIT 1")->fetchrow();
	
	
	foreach($res as $k => $v)
		echo "$k $v\n";

	return false;
}

function submit($data){
	global $db;

	$db->pquery("INSERT INTO results SET baseline = ?, player = ?, size = ?, time = ?, weight = 1, wins = ?, losses = ?, ties = ?, numgames = 1
		ON DUPLICATE KEY UPDATE wins = wins + ?, losses = losses + ?, ties = ties + ?, numgames = numgames + 1",
		$data['baseline'], $data['player'], $data['size'], $data['time'],
		(int)($data['outcome'] == 2), (int)($data['outcome'] == 1), (int)($data['outcome'] == 0),
		(int)($data['outcome'] == 2), (int)($data['outcome'] == 1), (int)($data['outcome'] == 0));

	$db->pquery("INSERT INTO games SET  baseline = ?, player = ?, size = ?, time = ?, timestamp = ?, outcome = ?, log = ?, host = ?",
		$data['baseline'], $data['player'], $data['size'], $data['time'], time(), $data['outcome'], $data['log'], gethostbyaddr($_ENV["REMOTE_ADDR"]));

	echo "1";
	return false;
}

function resetresults($data){
	global $db;

	$db->query("TRUNCATE results");
	$db->query("INSERT INTO results
		SELECT
			baseline,
			player,
			size,
			time,
			1 as weight,
			SUM(outcome = 2) as wins,
			SUM(outcome = 1) as losses,
			SUM(outcome = 0) as ties,
			count(*) as numgames
		FROM games 
		GROUP BY baseline, player, size, time");
}

