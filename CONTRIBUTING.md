# CONTRIBUTING

PRs and issues are welcome. If you are in doubt, if some change is even needed, then open an issue to discuss it. Is you are confident enough, there is nobody to stop you to introduce changes via PR without initial discussion, but there is always a chance of doing useless job, so, consider the first option.

Also be polite to other people and blablabla.

## Running examples

Just type:

```shell
xmake run example_NAME
```

## One devenv to rule them all

This project uses [devenv](https://github.com/cachix/devenv). Long story short, this is a tool which helps to ensure all developers have the same versions of all essential tools. And this is important since clang-tidy and clang-format produce different output depending on version. And this is checked in CI (because code formatting is automatable robot job, not an art)

To enter devenv run

```shell
devenv shell [command]
```

Where optional command param may be replaced with editor of your choice.

If somehow environment is not reproducible on your machine by just running suggested command, consider opening an Issue or/and PR.

## Actual workflow

This project uses [xmake](https://github.com/xmake-io/xmake) as it's build system, dependency manager, and workflow driver (whatever is that).

Here is the list of commands you need to use to pass CI

Select the config:

```shell
xmake config --toolchain=(gcc|clang++) --mode(debug|asan|tsan|release)
```

You will need to make tests pass in all of them. 99% of the cases passing it in tsan mode guarantees passage with all other modes and toolchains.

Run tests:

```shell
xmake test
```

And, if you are an LSP-slave, like me

```shell
xmake project -k compile_commands
```

Then run codestyle-related checks:

```shell
xmake format
xmake check clang.tidy
```
