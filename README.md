# Caller

A small helper library that allows me to call a SIP number and play a wav-file.

```elixir
Caller.call("100", "hello.wav", server: "mysipserver", user: "100", password: "test", caller_id: "100")
```

Or you can use the `caller` executable found in the `priv` directory directly:

```
USAGE: caller [options]

Options:
  --help                          Print usage and exit.
  --sound=wav-file, -s wav-file   The message to play.
  --id=id, -i id                  The SIP is used to originate the call from.
  --reg_uri=uri, -r reg_uri       The SIP URI where to register.
  --proxy=uri                     The SIP URI of the proxy.
  --realm=realm_name              The authentication realm.
  --user=user_id, -u user_id      The SIP user to authenticate with.
  --pw=password, -p password      The password to authenticate with.
  --dst_uri=sip, -d sip           The SIP uri to call.
  --loglevel=1                    The log level for debugging.

```
