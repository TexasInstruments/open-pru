<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# Release Notes

</div>

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

## Compatible SDK releases

These SDK versions can be used to build projects in each tag release:

### v2025.00.00

| SDK | am243x | am261x | am263px | am263x | am62x | am64x|
| --- | ------ | ------ | ------- | ------ | ----- | ---- |
| MCU+ SDK | 10.1, 11.0 | 10.0.1 | 10.1 | 10.0, 10.1 | N/A | 10.1, 11.0 |

These new SDK versions are not supported yet:

| SDK | am243x | am261x | am263px | am263x | am62x | am64x|
| --- | ------ | ------ | ------- | ------ | ----- | ---- |
| MCU+ SDK | 11.1 | 10.2 | 10.2 | 10.2 | N/A | 11.1 |
| Linux SDK | N/A | N/A | N/A | N/A | not yet | not yet |

Older SDK versions are not supported with the open-pru infrastructure:

| SDK | am243x | am261x | am263px | am263x | am62x | am64x|
| --- | ------ | ------ | ------- | ------ | ----- | ---- |
| MCU+ SDK | 10.0 or earlier |  | 10.0 or earlier | 9.2 or earlier | N/A | 10.0 or earlier |

