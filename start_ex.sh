echo "Activate python virtual env..."
source ~/dbimpl/.venv/bin/activate

echo "Meson setup ..."
cd ~/dbimpl/example
uv run meson setup pgvecbuild --prefix=~/dbimpl/pgvecinstall
deactivate
read -p "Press enter to continue"

echo "Ninja compiling ..."
cd ~/dbimpl/example/pgvecbuild
ninja
ninja install
read -p "Press enter to continue"

if [ -d ~/dbimpl/pgvecinstall/data ]; then
    ~/dbimpl/pgvecinstall/bin/pg_ctl -D ~/dbimpl/pgvecinstall/data -l logfile stop
    rm -rf ~/dbimpl/pgvecinstall/data
fi

echo "Initial Database"
~/dbimpl/pgvecinstall/bin/initdb -D ~/dbimpl/pgvecinstall/data
~/dbimpl/pgvecinstall/bin/pg_ctl -D ~/dbimpl/pgvecinstall/data -l logfile start
~/dbimpl/pgvecinstall/bin/createdb test
# ~/dbimpl/pgvecinstall/bin/psql test
~/dbimpl/example/pgvecbuild/src/test/regress/pg_regress --inputdir=/home/florenz/dbimpl/example/src/test/regress   hnsw_regression
