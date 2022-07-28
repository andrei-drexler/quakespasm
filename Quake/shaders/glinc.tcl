#!/bin/env tclsh

set ignore {}

proc process_shader {filename} {
    set filename [file normalize $filename]
    set shader_dir [file dirname $filename]

    if { [dict exists $::ignore $filename] } {
        return {1}
    }

    try {
        set shader_chan [open $filename]
    } trap {} {} {
        return [list 0 "Can't open file $filename"]
    }

    dict set ::ignore $filename ""
    set result {1}

    while { [gets $shader_chan line] >= 0 && [lindex $result 0] } {
        set match [regexp\
            {^\s*@\s*include_once\s*"([^"]*)"\s*$}\
            $line\
            _\
            next_file\
        ]
        
        if { $match } {
            if { $next_file == "" } {
                result = [list 0 "Bad line \"$whole\" in $filename"]
            } else {
                set next_file [file join $shader_dir $next_file]
                set result [process_shader $next_file]
            }
        } else {
            puts $line
        }
    }

    close $shader_chan

    return $result
}

proc main {} {
    if { $::argc != 1 } {
        puts stderr "Expected 1 argument, found $::argc"
        exit 1
    }

    set filename [lindex $::argv 0]
    set result [process_shader $filename]
    
    if { ![lindex $result 0] } {
        puts stderr [lindex $result 1]
        exit 1
    }
}

main
