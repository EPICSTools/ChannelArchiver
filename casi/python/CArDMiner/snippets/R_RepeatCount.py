# $Id$
#
########################################################
#### This snippet simply skips values that have a
#### REPEAT or EST_REPEAT count greater than maxRepeatCount
########################################################
   
#$GLBL
from re import search
from string import atoi
from time import asctime, gmtime, time
mark_in = time()
the_logger.AddLine("Mark In: " + asctime(gmtime(mark_in)))
value_count = 0
RepeatCounter = 0
maxRepeatCount = 1000
#$END_GLBL

########################################################

#$INIT
chan_name = chanI.name()
#$END_INIT

########################################################

#$BODY
value_string = valI.status()
value_time = valI.get()

match = search(r'(?:Repeat{1,1}[" "]+)(\d+)', value_string)

try:
   if atoi(match.groups()[0]) > maxRepeatCount:
      skip_value = "YES"
      RepeatCounter = RepeatCounter + 1

except AttributeError:
   skip_value = "NO"
   value_count = value_count + 1

#$END_BODY

########################################################

#$POST
the_logger.AddLine("Skipped " + str(RepeatCounter) + " values because repeat count exceeded " + str(maxRepeatCount))
the_logger.AddLine("Added a total of " + str(value_count) + " values")
mark_out = time()
the_logger.AddLine("Mark Out: " + asctime(gmtime(mark_out)))
et = mark_out - mark_in
secs = int(et%60)
mins = int((et/60)%60)
hours = int(et/3600)
the_logger.AddLine("Archive Sythesis took: " + str(hours) + ":" + str(mins) + ":" + str(secs) + " hours:mins:secs")
#$END_POST








