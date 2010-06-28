<?
error_reporting(E_ALL);

include("router.php");

$router = new PHPRouter();

$router->add("GET", "/", "home.php", "home", null);
$router->add("GET", "/info", "home.php", "info", null);
$router->add("GET", "/get", "home.php", "get", array("test" => "string"));
$router->add("POST","/post", "home.php", "post", array("test" => "string"));

$router->add("GET", "/results", "results.php", "askresults", array("baselines" => "array", "times" => "array", "players" => "array", "errorbars" => "bool", "data" => "bool"));
$router->add("POST","/results", "results.php", "showresults", array("baselines" => "array", "times" => "array", "players" => "array", "errorbars" => "bool", "data" => "bool"));
$router->add("GET", "/results/hosts", "results.php", "gethosts", null);
$router->add("GET", "/results/recent", "results.php", "getrecent", null);


$router->add("GET","/api/getwork", "api.php", "getwork", null);
$router->add("POST","/api/submit", "api.php", "submit", array("baseline" => "int", "player" => "int", "size" => "int", "time" => "int", "outcome" => "int", "log" => "string"));


$route = $router->route();

ob_start();
if($route->file)
	require($route->file);
$ret = call_user_func($route->function, $route->data);
$body = ob_get_clean();

if($ret)
	echo "<html><head><title>Havannah</title></head><body>\n$body\n</body></html>\n";
else
	echo $body;

exit;
