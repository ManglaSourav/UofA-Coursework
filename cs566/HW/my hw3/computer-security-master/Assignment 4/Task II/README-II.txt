FIRSTNAME: Prathyusha
LASTNAME:  Butti
EMAILADDRESS: pbutti@email.arizona.edu
UNDERGRADUATE/MASTERS/PHD : M
SCRIPT1:  I wanted to try Virtualize and EncodeArithmetic. Virutalize turned my functions into interpreter and EncodeArithmetic replaced my integer arithmetic with more complex ones. After looking at my obfuscated code I realized that none of the functions can be matched with original one as it has been transformed hence the user will find tough time to deobfuscate it. So it is well protected. When I executed "time ./out1" I observed the overhead in execution to be around 50%.
SCRIPT2:  I wanted to try InitEntropy, InitOpaque and AddOpaque. Add Opaque splits up my control flow by adding opaque branches. InitOpaque creates types and variables to add opaque predictes. InitEntropy introduces randomness. After looking at my obfuscated code I realized that none of the functions can be matched with original one as it has been transformed and as you cannot make out the flow, it is protected. When I executed "time ./out2" I observed the overhead in execution to be around 100%.
SCRIPT3:  I wanted to try Flatten Transformation extensively. Hence used Flatten on pretty much every function in addition to InitEntropy, IntiOpaque on encrypt functions. After looking at my obfuscated code I realized that every function was broken into switch cases and its highly impossible to deobfuscate without knowing the control flow of the program and as you cannot make out the flow it is well protected. The number of pointer castings confuses the user. When I executed "time ./out3" I observed the overhead in execution to be around 500%.
COMMENTS: I enjoyed it. My first time obfuscating the code with tigress was super interesting.


