<?php

global $has_pygmentize;
if (exec("which pygmentize 2>1 &", $o, $r) == '') $has_pygmentize = false;
else $has_pygmentize = true;

function colorize($str, $color, $bold = false) {
    $start = "\033[";
    if ($bold) $start .= "1;";
    else $start .= "0;";
    switch ($color) {
        case 'black':
            $start .= "30";
            break;
        case 'blue':
            $start .= "34";
            break;
        case 'green':
            $start .= "32";
            break;
        case 'red':
            $start .= "31";
            break;
        case 'gray':
            $start .= "37";
            break;
        case 'cyan':
            $start .= "30";
            break;
        case 'purple':
            $start .= "35";
            break;
        default:
            $start .= "30";
            break;
    }
    return $start. 'm'. $str. "\033[0m";
}

function success() {
    return colorize(" [✓] ", 'green', true)."\n";
}

function fail($msg) {
    return colorize(" [✗] \n".$msg, 'red', true)."\n";
}

function standard_fail($db) {
    return fail("Error [{$db->errorno()}] {$db->error()}");
}

function display_code($path, $after, $till) {
    global $has_pygmentize;

    if ($has_pygmentize) {
        $cmd = "pygmentize -lphp ";
    } else {
        $cmd = "cat -b ";
    }
    $end = $till - 1;
    $len = $end - $after;
    passthru("$cmd $path|head -n $end |tail -n $len");
    echo "\n";
}

?>
