<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Release Notes

</div>

[Viewing the release notes](#viewing-the-release-notes)  
[Checking out a specific release](#checking-out-a-specific-release)  
[Compatible SDKs](#compatible-sdks)  
[Compatible versions of CCS & other tools](#compatible-versions-of-ccs--other-tools)  

The open-pru repo does not have official software releases like the TI SDKs.

Instead, we will regularly update the open-pru repo. When the repo has
been significantly changed, we will add a new tag to the main branch.
The tag's message will include a list of updates since the previous tag.

## Viewing the release notes

To see the full release notes, use command
```
$ git tag --sort=-taggerdate -n99
```

## Checking out a specific release

To base your development on a specific tag (instead of the tip of the main
branch), checkout the tag like
```
$ git checkout -b my-branch-name tag-name
```

## Compatible SDKs

These SDK release versions can be used to build OpenPRU projects with a specific
tag. The OpenPRU projects may require modifications before they can be built
with older SDK versions. For more information, refer to
[Using Older SDKs with OpenPRU](./using_older_sdks_with_open_pru.md).

<details>
  <summary>v2025.00.00</summary>

  | SDK      | am243x      | am261x    | am263px   | am263x    | am62x | am64x       |
  | -------- | ----------- | --------- | --------- | --------- | ----- | ----------- |
  | MCU+ SDK | 10.1 - 11.0 | 10.2 only | 10.2 only | 10.2 only | N/A   | 10.1 - 11.0 |

</details>

## Compatible versions of CCS & other tools

**MCU+ SDK users:**
Use the CCS version, SysConfig version, and other tool versions
listed in the MCU+ SDK "Getting Started" docs for your selected MCU+ SDK
release. See [Getting Started with MCU+ SDK](./getting_started_mcuplus.md) for
details.

**Linux SDK users:**
Use the CCS and tool versions listed in
[Getting Started with Linux SDK](./getting_started_linux.md) when the open-pru
repo is checked out to the specific release tag that you are using.
