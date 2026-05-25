# hello_plugin example resolver plugin

`hello_plugin` is a minimal resolver plugin for the isched runtime. It demonstrates
how a shared library can register GraphQL resolvers at runtime through the plugin
system.

## What it provides

- Plugin id: `hello_plugin`
- Plugin kind: `resolver`
- GraphQL scope: `isched_examples`
- Example field: `echo(message: String!): String!`

The resolver simply returns the supplied `message` argument unchanged. If no
message is provided, it returns a friendly default string.

## Build

From the repository root:

```bash
python3 configure.py
cmake --build ./cmake-build-debug/ --target hello_plugin
```

The shared library is produced in the CMake build tree, typically at:

```text
cmake-build-debug/src/example/cpp/libhello_plugin.so
```

## Load into a running server

The loader script prompts for a platform-admin username and password, logs in via
GraphQL, and then calls the `loadPlugin` mutation.

```bash
python3 src/example/cpp/load_hello_plugin.py \
  --server-url http://127.0.0.1:18080/graphql \
  --plugin-path cmake-build-debug/src/example/cpp/libhello_plugin.so
```

If `--plugin-path` is omitted, the script searches common build directories for a
prebuilt `hello_plugin` library.

## Verify the resolver

Once loaded, the example query is:

```graphql
query {
  isched_examples {
    echo(message: "hello world")
  }
}
```

That query should return the same string in the `echo` field.

