set -x
set -e

sed -i /0\.22/d ~/.ssh/known_hosts
sed -i /0\.23/d ~/.ssh/known_hosts
sed -i /0\.24/d ~/.ssh/known_hosts
sed -i /0\.25/d ~/.ssh/known_hosts
sed -i /0\.26/d ~/.ssh/known_hosts

scp -o StrictHostKeyChecking=no core.o root@192.168.0.22:
#scp -o StrictHostKeyChecking=no core.o root@192.168.0.23:
#scp -o StrictHostKeyChecking=no core.o root@192.168.0.24:
#scp -o StrictHostKeyChecking=no core.o root@192.168.0.25:
#scp -o StrictHostKeyChecking=no core.o root@192.168.0.26:
