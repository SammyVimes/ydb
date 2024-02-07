# Development process: working on a change for YDB

This section contains a step-by-step scenario which helps you complete necessary configuration steps, and learn how to bring a change to the YDB project. This scenario does not have to be strictly followed, you may develop your own approach based on the provided information.

## Set up the environment {#envsetup}

### GitHub account {#github_login}

You need to have a GitHub account to suggest any changes to the YDB source code. Register at [github.com](https://github.com/) if haven't done it yet.

### SSH key pair {#ssh_key_pair}

Create an SSH key pair and register the public key at your GitHub account settings to be authenticated at GitHub when running commands from a command line on your development machine.

Full instructions are on [this GitHub page](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent#generating-a-new-ssh-key).

### Git CLI {#git_cli}

You need to have the `git` command-line utility installed to run commands from the console. Visit the [Downloads](https://git-scm.com/downloads) page of the official website for installation instructions.

To install it under Linux/Ubuntu run:

```
sudo apt-get update
sudo apt-get install git
```

### GitHub CLI {#gh_cli}

Using GitHub CLI enables you to create Pull Requests and manage repositories from a command line. You can also use GitHub UI for such actions.

Install GitHub CLI as described [at the home page](https://cli.github.com/). For Linux Ubuntu, you can go directly to [https://github.com/cli/cli/blob/trunk/docs/install_linux.md#debian-ubuntu-linux-raspberry-pi-os-apt](https://github.com/cli/cli/blob/trunk/docs/install_linux.md#debian-ubuntu-linux-raspberry-pi-os-apt).

Run authentication configuration:

```
gh auth login
```
You will be asked several questions interactively, answer them as follows:

|Question|Answer|
|--|--|
|What account do you want to log into?|**GitHub.com**|
|What is your preferred protocol for Git operations?|**SSH**|
|Upload your SSH public key to your GitHub account?|Choose a file with a public key (extention `.pub`) of those created on the ["Create SSH key pair"](#ssh_key_pair) step, for instance **/home/user/.ssh/id_ed25519.pub**|
|Title for your SSH key|**GitHub CLI** (leave default)|
|How would you like to authenticate GitHub CLI|**Paste your authentication token**|

After the last answer, you will be asked for a token which you can generate in the GitHub UI:

```
Tip: you can generate a Personal Access Token here https://github.com/settings/tokens
The minimum required scopes are 'repo', 'read:org', 'admin:public_key'.
? Paste your authentication token:
```

Open the [https://github.com/settings/tokens](https://github.com/settings/tokens), click on "Generate new token" / "Classic", tick FOUR boxes:
* **Box `workflow`**
* Three others as adivised in the tip: "repo", "admin:public_key" and "read:org" (under "admin:org")

And copy-paste the shown token to complete the GitHub CLI configuration.

### Fork and clone repository {#fork_create}

YDB official repository is [https://github.com/ydb-platform/ydb](https://github.com/ydb-platform/ydb), located under the YDB organization account `ydb-platform`.

To work on the YDB code changes, you need to create a fork repository under your GitHub account, and clone it locally. Create a fork pressing `Fork` button on the official
repository page.

After your fork is set up, clone the official repository:
```
mkdir -p ~/ydbwork
cd ~/ydbwork
git clone https://github.com/ydb-platform/ydb.git
```

Once completed, you have a YDB Git repository cloned to `~/ydbwork/ydb`.

Forking a repository is an instant action, however cloning to the local machine takes some time to transfer about 650 MB of repository data over the network.

Now add your fork as a remote:
```
git remote add fork https://github.com/{your_user_name}/ydb.git
```

### Configure commit authorship {#author}

Run the following command from your repository directory to set up your name and email for commits pushed using Git:

```
cd ~/ydbwork/ydb
```
```
git config user.name "Marco Polo"
git config user.email "marco@ydb.tech"
```

## Working on a feature {#feature}

To start working on a feature, ensure the steps specified in the [Setup the environment](#envsetup) section above are completed.

### Refresh trunk {#fork_sync}

Usually you need a fresh trunk revision to branch from. Sync your local main branch running the following command in the repository:

If your current local branch is not `main`:
```
cd ~/ydbwork/ydb
git fetch origin main:main
```
This updates your local main branch without checking it out.

If your current local branch is `main`:
```
git pull --ff-only origin main
```

### Create a development branch {#development_branch}

Create a development branch using Git (replace "feature42" with your branch name), and assign upstream for it:

```
git checkout -b feature42
git push --set-upstream fork feature42
```

### Make changes and commits {#commit}

Edit files locally, use standard Git commands to add files, verify status, make commits, and push changes to your fork repository:

```
git add .
```

```
git status
```

```
git commit -m "Implemented feature 42"
```

```
git push
```

### Create a pull request to the official repository {#create_pr}

When the changes are completed and locally tested (see [Ya Build and Test](build-ya.md)), visit your branch's page on GitHub.com, press `Contribute` and then `Open Pull Request`.
You can also use the link in the `git push` output to open a Pull Request.

### Precommit checks {#precommit_checks}

Prior to merging, the precommit checks are run for the Pull Request. You can see its status on the Pull Request page.

As part of the precommit checks, the YDB CI builds artifacts and runs all the tests, providing the results as a comment to the Pull Request.

If you are not a member of the YDB team, build/test checks do not run until a team member reviews your changes and approves the PR for tests by assigning a label 'Ok-to-test'.

### Test results {#test-results}

You can click on the test amounts in different sections of the test results comment to get to the simple HTML test report. In this report you can see which tests have been failed/passed, and get to their logs.

### Test history {#test_history}

Each time when tests are run by the YDB CI, their results are uploaded to the [test history application](https://nebius.testmo.net/projects/view/1). There's a link "Test history" in the comment with test results heading to the page with the relevant run in this application.

In the "Test History" YDB team members can browse test runs, search for tests, see the logs, and compare them between different test runs. If some test is failed in a particular precommit check, it can be seen in its history if this failure had been introduced by the change, or the test had been broken/flaky earlier.

### Review and merge {#review}

The Pull Request can be merged after obtaining an approval from the YDB team member. Comments are used for communication. Finally a reviewer from the YDB team clicks on the 'Merge' button.

### Update changes {#update}

If there's a Pull Request opened for some development branch in your repository, it will update every time you push to that branch, restarting the checks.

### Rebase changes {#rebase}

If you have conflicts on the Pull Request, you may rebase your changes on top of the actual trunk from the official repository. To do so, [refresh trunk](#fork_sync) in your fork, pull the `main` branch state to the local machine, and run the rebase command:

```
# Assuming your active branch is your development branch
git fetch origin main:main
git rebase main
```
