#docker run -it  -v $(pwd):/work -w /work python:3.8 bash
#docker run -it -v $(pwd):/work -w /work apihackers/twine
# twine upload -r https://miil-pypi-ais.alibaba-inc.com/miil/stable min_tfs_client-1.0.6-py3-none-any.whl
./scripts/travis/protoc_install.sh
mkdir tensor_serving_client
mv ~/bin/protoc /usr/local/bin/
mv ~/include/google/ protobuf_srcs/
python setup.py bdist_wheel