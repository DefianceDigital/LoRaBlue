input = "firmware.uf2"                                                             # Input File
output = "firmware.txt"                                                           # Output File:

delim = 1                                                                       # Insert separator every n bytes
newline = 10                                                                    # Insert newline every n delimiters

# -------------------

delim = delim * 2                                                               # Bytes in hex are 2 characters long

f=open(input,"rb")
o=open(output,"w")

content=f.read().hex()                                                          # Read file as hex bytes

o.write(content)
o.close()
f.close()