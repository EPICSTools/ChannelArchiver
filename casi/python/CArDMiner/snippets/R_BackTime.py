# $Id$
#
########################################################
#### This snippet simply skips values that go back in
#### time
########################################################
	
#$GLBL
#$END_GLBL

#########################################################

#$INIT
value_time = valI.time()  ### is set each channel to first timestamp
chan_name = chanI.name()  ### is set each channel
#$END_INIT

#########################################################

#$BODY
try:
	value_prev_time = value_time
except NameError:
	value_prev_time = 0

value_time = valI.time()

if value_prev_time > value_time:
	skip_value = "YES"

#### To suppress entry to log when value is skipped comment
#### out the next two lines (add #'s)
	the_logger.AddLine("	 Skipped " + value_time + " in " + chan_name)
	the_logger.AddLine("	 REASON: Went back in time")

#$END_BODY

#########################################################

#$POST
#$END_POST
