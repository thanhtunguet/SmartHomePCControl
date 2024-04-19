SmartHomePCControl
------------------

Control your Home PC using Google Smart Home assistant

## Installation

### Quick setup --- if you've done this kind of thing before

[ Set up in Desktop](x-github-client://openRepo/https://github.com/thanhtunguet/SmartHomePCControl)

### ...or create a new repository on the command line

```sh
echo "# SmartHomePCControl" >> README.md
git init
git add README.md
git commit -m "first commit"
git branch -M main
git remote add origin git@github.com:thanhtunguet/SmartHomePCControl.git
git push -u origin main
```

### ...or push an existing repository from the command line

```sh
git remote add origin git@github.com:thanhtunguet/SmartHomePCControl.git
git branch -M main
git push -u origin main
```

### ...or import code from another repository

You can initialize this repository with code from a Subversion, Mercurial, or TFS project.

[Import code](https://github.com/thanhtunguet/SmartHomePCControl/import)

## Usage

Running the project using docker compose

```yml
version: '3'
services:
  smart-home-pc:
    image: thanhtunguet/smart-home-pc:latest
    network_mode: host
    container_name: smart-home-pc
    command: dotnet SmartHomePCControl.dll --urls http://0.0.0.0:5000
```
