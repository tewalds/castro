<?

function home($data){
?>
<h1>This is the body!</h1><br>
Get: <form method="get" action="/get"><input name="test"><input type="submit"></form><br>
Post: <form method="post" action="/post"><input name="test"><input type="submit"></form><br>
<?
	return true;
}

function info($data){
	phpinfo();

	return false;
}

function get($data){
	print_r($data);
	return true;
}

function post($data){
	print_r($data);
	return true;
}
