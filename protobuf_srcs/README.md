## TF Protobuf definitions

This directory contains the forked subtrees of the `tensorflow` and `tensorflow_serving` projects which provide the project with TensorFlow protobuf definitions.

They should only be updated to pull changes from the original upstream projects using `git subtree` commands.

### How to update the Protobuf definitions (to pull changes from upstream) 

#### Set up the upstream (do this the first time you want to update)
1. Clone this repo
2. Set the upstream remote to the parent [tensorflow repo](https://github.com/tensorflow/tensorflow) and/or [tensorflow_serving repo](https://github.com/tensorflow/serving)
```
git remote add upstream-tf git@github.com:tensorflow/tensorflow.git
git remote add upstream-serving git@github.com:tensorflow/serving.git
```
This should set up a new master branch for each upstream (`upstream-tf/master` and `upstream-serving/master`) and copy all of the upstream refs into your local. 

#### Updating with upstream changes
This is a very similar process for `tensorflow` and `tensorflow_serving` - the main difference is that `tensorflow` is much bigger than `tensorflow_serving`, so it will take longer (possibly up to an hour).

##### tensorflow
1. Pull the updated upstream refs:
```
git fetch --depth 1 upstream-tf
```
Now `upstream-tf` is up to date.

Also fetch the upstream tags (since we use two upstreams, it's a good idea to use a [refspec](https://stackoverflow.com/a/22120725) to keep tags organised.

```
git fetch upstream-tf '+refs/tags/*:refs/rtags/upstream-tf/*'
```

2. Check out the ref you want to update to:
```
git checkout refs/rtags/upstream-tf/<version>
```

3. Create a `upstream-tf-subtree` branch containing the subtree from `upstream-tf`. This command will take a while - `tensorflow/tensorflow` is _big_.
```
git subtree split --prefix tensorflow --branch upstream-tf-subtree
```

4. Create a new branch in the `min-tfs-client` tree with clean `protobuf_srcs/tensorflow`.

There doesn't appear to be a way to use `git subtree` commands to cleanly pull the subtree changes into a branch, especially since tensorflow repos do their own fancy git rework which complicates things. Therefore the recommended approach is to use base `git` to ensure a clean history.

```
git checkout master
git checkout -b <new-branch>
rm -rf protobuf_srcs/tensorflow
git add --all && git commit -m "[WIP]"
```

5. Merge the subtree into `protobuf_srcs`:
```
git subtree add --squash --prefix protobuf_srcs/tensorflow upstream-tf-subtree
```

5. Clean the commit history:
```
git reset --soft master
git add protobuf_srcs/tensorflow && git commit
```

It helps to include a descriptive PR commit message which mentions the upstream git ref used, e.g.
```
Squashed commit of tensorflow changes

tensorflow/tensorflow commit [<version>]: <commit SHA>
```

6. Push and raise a PR for `<new-branch>` against `master`
```
git push -u origin HEAD
```

##### tensorflow_serving
1. Pull the updated upstream refs:
```
git fetch --depth 1 upstream-serving
```
Now `upstream-serving` is up to date.

Also fetch the upstream tags (since we use two upstreams, it's a good idea to use a [refspec](https://stackoverflow.com/a/22120725) to keep tags organised.

```
git fetch upstream-serving '+refs/tags/*:refs/rtags/upstream-serving/*'
```

2. Check out the ref you want to update to:
```
git checkout refs/rtags/upstream-serving/<version>
```

3. Create a `upstream-serving-subtree` branch containing the subtree from `upstream-serving`.
```
git subtree split --prefix tensorflow_serving --branch upstream-serving-subtree
```

4. Create a new branch in the `min-tfs-client` tree with clean `protobuf_srcs/tensorflow_serving`.

There doesn't appear to be a way to use `git subtree` commands to cleanly pull the subtree changes into a branch, especially since tensorflow repos do their own fancy git rework which complicates things. Therefore the recommended approach is to use base `git` to ensure a clean history.

```
git checkout master
git checkout -b <new-branch>
rm -rf protobuf_srcs/tensorflow_serving
git add --all && git commit -m "[WIP]"
```

5. Merge the subtree into `protobuf_srcs`:
```
git subtree add --squash --prefix protobuf_srcs/tensorflow_serving upstream-serving-subtree
```

5. Clean the commit history:
```
git reset --soft master
git add protobuf_srcs/tensorflow_serving && git commit
```

It helps to include a descriptive PR commit message which mentions the upstream git ref used, e.g.
```
Squashed commit of tensorflow serving changes

tensorflow/tensorflow_serving commit [<version>]: <commit SHA>
```

6. Push and raise a PR for `<new-branch>` against `master`
```
git push -u origin HEAD
```

