sudo apt-get install libsnappy-dev

wget http://pkgs.fedoraproject.org/repo/pkgs/leveldb/leveldb-1.9.0.tar.gz/12f11385cb58ae0de66d4bc2cc7f8194/leveldb-1.9.0.tar.gz
tar -xzf leveldb-1.9.0.tar.gz
cd leveldb-1.9.0
make
sudo mv libleveldb.* /usr/local/lib
cd include
sudo cp -R leveldb /usr/local/include
sudo ldconfig
