
import sys

filename = sys.argv[1]

fp = open(filename, "rb")
data = fp.read()


fp.close()

new_dat = [((data[i] & 0xF0) >> 4) | ((data[i] & 0xF) << 4) for i in range(len(data))]

out = open("out.4bppwii", "wb")
out.write(bytearray(new_dat))
out.close()

