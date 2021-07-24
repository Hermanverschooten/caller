defmodule Caller do
  require Logger

  def call(destination, soundfile, config) do
    sipserver = config[:server]
    user = config[:user]
    password = config[:password]
    caller_id = config[:caller_id]

    exe = Application.app_dir(:caller, "priv/caller")

    System.cmd(exe, [
      "-s",
      soundfile,
      "-i",
      id(caller_id, sipserver),
      "-r",
      "sip:" <> sipserver,
      "-u",
      user,
      "-p",
      password,
      "-d",
      id(destination, sipserver)
    ])
  end

  def id(user, sipserver) do
    ["sip:", user, "@", sipserver] |> Enum.join()
  end
end
