#!/bin/sh

prog=$0
Usage()
{
    echo "$prog [-p <port>] [-t <test>] <progname> -- <args to simulator>"
    echo "   if -t is NOT specified, run <progname>"
    echo "   if -t is specified, run <test>"
    exit 1;
}

port=5000
test=0
type -t meldvm > /dev/null
if [ $? == 1 ]; then
    echo "I can't find the meldvm"
    exit -1
fi

while [ "$#" -gt "0" ]
do
    if [ ${1#-} == $1 -o "$1" == "--" ]; then
	break;
    fi
    case $1 in
        -p)
            #set port to use
            shift
            port=$1
            ;;
	-t)
	    # run a test
            shift
	    test=$1
	    if [ ! -e $test ]; then
		echo "Can't find the test spec: $test"
		exit 1
	    fi
	    ;;
        -h)
            Usage
            ;;
        *)
            echo "$prog: Bad argument $1"
            Usage
            ;;
    esac
    shift
done

meldfile=0
if [ $# -gt 0 ]; then
    if [ ${1#-} == $1 ]; then
    # this is prog name
	meldfile=$1
	if [ ! -e $meldfile ]; then
	    echo "Can't find the meld bytecode file: $meldfile"
	    exit 1
	fi
	shift
    fi
fi

if [ "$meldfile" == "0" -a "$test" == "0" ]; then
    echo "Must have -t or a meld bytecode file"
    Usage
fi

simargs=""
if [ "$1" == "--" ]; then
    # args to the simulator
    shift
    simargs=$*
fi

echo $meldfile
if [ "$meldfile" == "0" ]; then
    # run a test
    echo "runing a test: $test"
    simargs="-t $test -i 10 $simargs"
    meldfile=`grep "^meld:" $test | sed -e 's/.*: *//'`
    if [ ! -e $meldfile ]; then
	echo "Can't find the meld bytecode file: $meldfile"
	exit 1
    fi
fi

# run a program
echo "./sim $simargs > sim.log 2>&1 &"
./sim $simargs > sim.log 2>&1 &
sleep 1
echo "meldvm -p $port -f $meldfile"
meldvm -p $port -f $meldfile
if [ "$test" != "0" ]; then
    grep "Test Passed" sim.log
    if [ $? == 0 ]; then
	echo "Test Passed!"
	exit 0
    else
	echo "Test failed"
	exit 1
    fi
fi


