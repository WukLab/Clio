set -x
set -e

sed -i /0\.22/d ~/.ssh/known_hosts
sed -i /0\.23/d ~/.ssh/known_hosts

#scp -o StrictHostKeyChecking=no core.o root@192.168.0.22:
scp -o StrictHostKeyChecking=no core.o root@192.168.0.23:
