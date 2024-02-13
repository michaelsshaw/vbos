usrprog=('ftest' 'test1' 'vfs')

for prog in ${usrprog[@]}; do
	make -C $prog $1 --no-print-directory
done
