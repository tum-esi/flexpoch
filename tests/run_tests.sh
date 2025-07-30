#! /bin/env bash

# ensure correct working dir
cd tests > /dev/null 2>&1 
cd ..

# init assertions
source "./tests/assert.sh"
expected=""
actual=""
test_failed=false

test () {
    # args: fp params, expected val, error msg
    actual="$(./bin/fp $1 | xargs)"
    expected="$2"
    # echo "act: $actual, exp: $expected"
    assert_eq "$expected" "$actual" "not equivalent! (./bin/fp $1, $3)"
    if [ "$?" -gt "0" ]; then
        test_failed=true
    fi
}

test_status () {
    if $test_failed; then
        echo $'\n'
        test_failed=false
    else
        echo $'All tests passed successfully.\n\n'
    fi
}

# run tests

###############
### Default ###
###############

# 40 b
# 0b1110_0001/0xE1 s[32] - 0b0111_1110/0x7E s[32]

# 24 b
# XX0: 119ns precision: 23 bits [23:1] for value, 1 bit [0] pattern
# 001: us precision: 20 bits [23:4] for value, 1 bit [3] unused, 3 bits [2:0] pattern
# 011: 1/2^15 precision: 15 bits [23:9] for value, 6 bits [8:3] unused, 3 bits [2:0] pattern
# 101: ms precision, 10 bits [23:14] for value, 11 bits [13:3] for timezone in minutes, 3 bits [2:0] pattern
# 111: s precision, 11 bit [23:13] timezone, 6 bits [12:7] unused, 4 bits [6:3] precision encoding, 3 bits [2:0] pattern

# leap second: [23:0] all 1
# TZ offset: UTC time stored + TZ offset

### Test normal values ###
echo "Test default behavior..."
echo "------------------------"

test "--from-unix 1743154226 --to-iso" "2025-03-28T09:30:26Z"
test "--from-unix 1743154226 --to-fp" "0x0067E66C32800007"
test "--from-iso 2025-03-28T09:30:26 --to-unix" "1743154226"
test "--from-iso 2025-03-28T09:30:26 --to-fp" "0x0067E66C32800007"
test "0x0067E66C32000000 --to-unix" "1743154226"
test "0x0067E66C32000000 --to-iso" "2025-03-28T09:30:26.000000000Z"

test "--from-unix 0 --to-iso" "1970-01-01T00:00:00Z"
test "--from-unix 0 --to-fp" "0x0000000000800007"

test "--from-unix 545460846591" "0x7EFFFFFFFF800007"  # max value
test "--from-unix 545460846592" "Error! Value outside of encodable time range."  # max value + 1

test_status

### Test precision values ###
echo "Test precision behavior..."
echo "--------------------------"

# 110ns precision
test "0x005F7AFF4F2AAAA8 --to-iso" "2020-10-05T11:11:11.166666508Z"
test "0x005F7AFF4F2AAAA8 --to-unix" "1601896271"
test "--from-iso 2020-10-05T11:11:11.166666508Z --to-fp" "0x005F7AFF4F2AAAA8"

# 1us precision
test "0x005F7AFF4F5BBBB1 --to-iso" "2020-10-05T11:11:11.358333Z"    # bit 3 = 0 (not used)
test "0x005F7AFF4F5BBBB9 --to-iso" "2020-10-05T11:11:11.358333Z"    # bit 3 = 1 (not used)
test "0x005F7AFF4F5BBBB9 --to-unix" "1601896271"
test "--from-iso 2020-10-05T11:11:11.358333Z --to-fp" "0x005F7AFF4F5BBBB1"

# 1/2^15 precision (RTC)
test "0x005F7AFF4F400003 --to-iso" "2020-10-05T11:11:11.250000Z"
test "0x005F7AFF4F4001FB --to-iso" "2020-10-05T11:11:11.250000Z"    # unused bits [8:3]

# 1ms precision
test "0x005F7AFF4F802005 --to-iso" "2020-10-05T11:11:11.500Z"         # +0min      (0b10000000000) / 0b1000 0000 00|10 0000 0000 0|101
test "0x005F7AFF4F80200D --to-iso" "2020-10-05T11:12:11.500+00:01"    # +1min      (0b10000000001) / 0b1000 0000 00|10 0000 0000 1|101
test "0x005F7AFF4F8021E5 --to-iso" "2020-10-05T12:11:11.500+01:00"    # +60min     (0b10000111100) / 0b1000 0000 00|10 0001 1110 0|101
test "0x005F7AFF4F8024B5 --to-iso" "2020-10-05T13:41:11.500+02:30"    # +150min    (0b10010010110) / 0b1000 0000 00|10 0100 1011 0|101
test "0x005F7AFF4F803FF5 --to-iso" "2020-10-06T04:13:11.500+17:02"    # +1022min   (0b11111111110) / 0b1000 0000 00|11 1111 1111 0|101
# test "0x005F7AFF4F803FFD --to-iso" "Leap second."                     # leapsecond (0b11111111111) / 0b1000 0000 00|11 1111 1111 1|101
# test "0x005F7AFF4FFFE007 --to-iso" "Leap second."                     # leapsecond (0b11111111111) / 0b1000 0000 00|11 1111 1111 1|101  bei second precision

test "0x005F7AFF4F801FFD --to-iso" "2020-10-05T11:10:11.500-00:01"    # -1min      (0b01111111111) / 0b1000 0000 00|01 1111 1111 1|101
test "0x005F7AFF4F801F8D --to-iso" "2020-10-05T10:56:11.500-00:15"    # -15min     (0b01111110001) / 0b1000 0000 00|01 1111 1000 1|101
test "0x005F7AFF4F80000D --to-iso" "2020-10-04T18:08:11.500-17:03"    # -1023min   (0b00000000001) / 0b1000 0000 00|00 0000 0000 1|101
test "0x005F7AFF4F800005 --to-iso" "2020-10-04T18:07:11.500-17:04"    # -1024min   (0b00000000000) / 0b1000 0000 00|00 0000 0000 0|101

# 1s precision (0b0000)
test "0x005F7AFF4F800007 --to-iso" "2020-10-05T11:11:11Z"       # +0min               (0b10000000000) / 0b1000 0000 000|X XXXX X|000 0|111
test "0x005F7AFF4F801F87 --to-iso" "2020-10-05T11:11:11Z"       # +0min (unused bits) (0b10000000000) / 0b1000 0000 000|1 1111 1|000 0|111
test "0x005F7AFF4F878007 --to-iso" "2020-10-05T12:11:11+01:00"  # +60min              (0b10000111100) / 0b1000 0111 100|X XXXX X|000 0|111
test "0x005F7AFF4F7E2007 --to-iso" "2020-10-05T10:56:11-00:15"  # -15min              (0b01111110001) / 0b0111 1110 001|X XXXX X|000 0|111

# 111: s precision, 11 bit [23:13] timezone, 6 bits [12:7] unused, 4 bits [6:3] precision encoding, 3 bits [2:0] pattern

# 1 min precision (0b0001)
test "0x005F7AFF4F80000F --to-iso" "2020-10-05T11:11Z"
test "0x005F7AFF4580000F --to-iso" "2020-10-05T11:11Z"   # different second values point to same min
test "--from-iso 2020-10-05T11:11 --to-fp" "0x005F7AFF4480000F"
test "--from-iso 2020-10-05T11:11Z --to-fp" "0x005F7AFF4480000F"

# 1 hour precision (0b0010)
test "0x005F7AFF4F800017 --to-iso" "2020-10-05T11Z"
test "0x005F7AFF7F800017 --to-iso" "2020-10-05T11Z"      # different second values point to same hour
test "--from-iso 2020-10-05T11 --to-fp" "0x005F7AFCB0800017"

# 1 day precision (0b0011)
test "0x005F7AFF4F80001F --to-iso" "2020-10-05"
test "0x005F7A881F80001F --to-iso" "2020-10-05"         # different second values point to same day
test "--from-iso 2020-10-05 --to-fp" "0x005F7A620080001F"

# 1 week precision (0b0100)
test "0x0067748580800027 --to-iso" "2025-W00"
test "0x0067778580800027 --to-iso" "2025-W00"           # different second values point to same week
test "--from-iso 2025-W00 --to-fp" "0x0067748580800027"                                                    

# 1 month precision (0b0101)
test "0x005F7AFF4F80002F --to-iso" "2020-10"
test "0x005F9AFF4F80002F --to-iso" "2020-10"            # different second values point to same month
test "--from-iso 2020-10 --to-fp" "0x005F751C0080002F"

# 1 quarter precision (0b0110)
test "0x005E43A7BB800037 --to-iso" "2020-Q1"                                                                 # => is Q defined in ISO?
# test "--from-iso 2020-Q1 --to-fp" "0x005E0BE100800037"                                                     # => Quarter not ISO standard

# 1 trimester precision (0b0111)
# test "0x005E43A7BB00003F --to-iso" "2020-R1"                                                               # => Trimester not ISO standard

# 1 semester precision (0b1000)
# test "0x005E43A7BB00003F --to-iso" "2020-S1"                                                               # => Semester not ISO standard

# 1 year precision (0b1001)
test "0x005F7AFF4F80004F --to-iso" "2020"
test "0x005F7AAA7F80004F --to-iso" "2020"               # different second values point to same year
test "--from-iso 2020 --to-fp" "0x005E0BE10080004F"

# 1 decade precision (0b1010)
test "0x005F7AFF4F800057 --to-iso" "202x"                                                                  # => Decade not shown

# 1 century precision (0b1011)
test "0x005F7AFF4F80005F --to-iso" "20xx"                                                                  # => Century not shown

# 1 millenium precision (0b1100)
test "0x005F7AFF4F800067 --to-iso" "2xxx"                                                                  # => Millenium not shown
# test "--from-iso 2020-10-05T11:11 --to-fp" "0x005F7AFF4F000067"

# unused precision (0b1101, 0b1110, 0b1111)
test "0x005F7AFF4F00006F --to-iso" "Error! Precision Error."
test "0x005F7AFF4F000077 --to-iso" "Error! Precision Error."
test "0x005F7AFF4F00007F --to-iso" "Error! Precision Error."

# Leap second
test "--from-iso 2016-12-31T23:59:59 --to-fp" "0x005868467F800007"  # :59 is second before (timezone UTC)
test "--from-iso 2016-12-31T23:59:60 --to-fp" "0x005868467FFFE007"  # :60 is timezone = FFE (11bit 1's)
test "--from-iso 2016-12-31T23:59:60Z --to-fp" "0x005868467FFFE007"
test "0x005868467FFFE007 --to-iso" "2016-12-31T23:59:60Z"
test "0x005868467FFFFF87 --to-iso" "2016-12-31T23:59:60Z"   # ignore extra bits on second precision

test "--from-iso 2016-12-31T23:59:59.000 --to-fp" "0x005868467F002005" # leapsecond at millisec prescision
test "--from-iso 2016-12-31T23:59:60.000 --to-fp" "0x005868467F003FFD" # leapsecond at millisec prescision
test "0x005868467F003FFD --to-iso" "2016-12-31T23:59:60.000Z"

test_status

############################
### Floating Point Years ###
############################

# 40 b
# > 1110_0000 [32b] float
# < 0111_1111 [32b] float

# 24 b
# all zeros

echo "Test floating point years behavior..."
echo "-------------------------------------"

### Test normal values ###
test "0x7EFFFFFFFF80001F --to-iso" "19254-12-19"    # last valid date with second
test "0x7F46966E00000000 --to-iso" "19255.0"        # first float year
test "0x7F469C4100000000 --to-iso" "20000.5"        # 20000.5
test "0x7F7F800000000000 --to-iso" "inf"            # inf
test "0x7F7FC00000000000 --to-iso" "nan"            # NaN

#1110_0001
#1110_0000

test "0xE10000000000001F --to-iso" "-2250-10-31-17:04"    # last negative valid date with second
test "0xE10000000080001F --to-iso" "-2250-11-01"    # last negative valid date with second (UTC)
test "0xE0C50CB000000000 --to-iso" "-2251.0"        # first negative float year
test "0xE0FF800000000000 --to-iso" "-inf"           # -inf
test "0xE0FFC00000000000 --to-iso" "-nan"           # NaN

### Test over/underflow ###
test "0x7F00000000000000 --to-iso" "Error! Invalid Floating Point Year."   # 0
test "0x7F3F800000000000 --to-iso" "Error! Invalid Floating Point Year."   # 1
test "0x7F46955A00000000 --to-iso" "Error! Invalid Floating Point Year."   # 19117
test "0x7F46955C00000000 --to-iso" "Error! Invalid Floating Point Year."   # 19118
test "0x7F46955C10000000 --to-iso" "Error! Invalid Floating Point Year."   # 19118.002

test "0xE0BF800000000000 --to-iso" "Error! Invalid Floating Point Year."    # -1


test_status



#####################
### Relative Time ###
#####################

# 4 b
# 1101

# 36 b
# seconds

# 24 b 
# fraction / precision

echo "Test relative time behavior..."
echo "-------------------------------------"

test "0xD000000000000000 --to-iso" "PT0.000000000S"                                  # => relative not properly implemented
test "0xD000015180000000 --to-iso" "PT86400.000000000S"     # 1 day = 86400s

# test "0xDFFFFFFFFE000000 --to-iso" ""   # -2386-09-25T08:55:27.000000000Z           => not relative                                            
# test "0xD7FFFFFFFF000000 --to-iso" ""   # -3475-12-01T05:09:19.000000000Z           => not relative                                            

test_status


#######################
### High-Resolution ###
#######################

# 4 b
# 1100

# 60 b
# microseconds?

# test "0xC000000000000000 " "highres"                                  # => highres not properly implemented
# test "0xCFFFFFFFFFFFFFFF" "highres"


####################
### Logical Time ###
####################

# 4 b
# 1010

# # 60 b
# logical
echo "Test logical time behavior..."
echo "-------------------------------------"
test "0xA000000000000000" "0"
test "0xA000000000000001" "1"
test "0xA100000000000000" "72057594037927936"
test "0xAFFFFFFFFFFFFFFF" "1152921504606846975"

test_status

###################
### Custom Time ###
###################

# 4 b
# 1011

# 60 b
# custom clock
echo "Test custom time behavior..."
echo "-------------------------------------"
test "0xB000000000000000" "Error! Custom codepoint range (start = 0xB). Please decode with custom decoder."

test_status

################
### Reserved ###
################

# 3 b
# 100

# 61 b
# reserved
echo "Test reserved range..."
echo "-------------------------------------"
test "0x8000000000000000 --to-unix" "Error! Reserved codepoint range (start = 0x8 or 0x9). Not supported by this version."                                  # => reserved not properly implemented
test "0x9FFFFFFFFFFFFFFF --to-unix" "Error! Reserved codepoint range (start = 0x8 or 0x9). Not supported by this version."

test_status
