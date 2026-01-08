echo "Activate python virtual env..."
source ~/dbimpl/.venv/bin/activate

echo "\nMeson setup ..."
cd ~/dbimpl/pgvec
uv run meson setup pgvecbuild --prefix=~/dbimpl/pgvecinstall
deativate

echo "\nNinja compiling ..."
cd ~/dbimpl/pgvec/pgvecbuild
ninja
ninja install

if [ -d ~/dbimpl/pgvecinstall/data ]; then
    ~/dbimpl/pgvecinstall/bin/pg_ctl -D ~/dbimpl/pgvecinstall/data -l logfile stop
    rm -rf ~/dbimpl/pgvecinstall/data
fi

echo "\nInitial Database"
~/dbimpl/pgvecinstall/bin/initdb -D ~/dbimpl/pgvecinstall/data
~/dbimpl/pgvecinstall/bin/pg_ctl -D ~/dbimpl/pgvecinstall/data -l logfile start
~/dbimpl/pgvecinstall/bin/createdb test
~/dbimpl/pgvecinstall/bin/psql test

