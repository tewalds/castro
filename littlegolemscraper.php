#!/usr/bin/php
<?

$outdir = "littlegolem/";

for($i = 1; $i < 1150000; $i++){
	if(file_exists($outdir . floor($i/1000) . "/$i.hgf"))
		continue;

	$file = file_get_contents("http://www.littlegolem.net/servlet/sgf/$i/game.hgf");
	
	if($file){
		if(!file_exists($outdir . floor($i/1000))){
			mkdir($outdir . floor($i/1000), 0777, true);
			echo "$i ";
		}

		file_put_contents($outdir . floor($i/1000) . "/$i.hgf", $file);
	}
	sleep(0.05);
}


