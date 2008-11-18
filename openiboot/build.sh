expr `cat build_num` "+" 1 > build_num
export BUILD="\"`cat build_num`\""
make

