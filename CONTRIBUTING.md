# Contributing to EVA Driver

Hi there!

We're thrilled that you'd like to contribute to EVA Driver.
Your help is essential for keeping this project great and making it better.

## Branching Strategy

In general, contributors should develop on branches based off of `eva-kernel.qclinux.0.0` and pull requests should be made against `eva-kernel.qclinux.0.0`.

## Submitting a Pull Request

1. Please read our [Code of Conduct](CODE-OF-CONDUCT.md) and [License](LICENSE.txt).

1. Fork and clone the repository.

    ```bash
    git clone https://github.com/<username>/eva-driver.git
    ```

1. Create a new branch based on `eva-kernel.qclinux.0.0`.

    ```bash
    git checkout -b <my-branch-name> eva-kernel.qclinux.0.0
    ```

1. Create an upstream remote to make it easier to keep your branches up to date.

    ```bash
    git remote add upstream https://github.com/qualcomm-linux/eva-driver.git
    ```

1. Make your changes, add tests where applicable, and ensure all checks pass.

1. Commit your changes using the DCO.

    ```bash
    git commit -s -m "Useful commit message"
    ```

1. Sync your branch with upstream before submission.

    ```bash
    git pull --rebase upstream eva-kernel.qclinux.0.0
    ```

1. Push to your fork.

    ```bash
    git push -u origin <my-branch-name>
    ```

1. Submit a pull request from your branch to `eva-kernel.qclinux.0.0`.

1. Wait for review and address any feedback received.

## Security Analysis of Pull Requests

To maintain the security and integrity of this project, pull requests may be automatically scanned to detect insecure coding patterns and potential security issues.

### Static Analysis

Automated analysis tools may be used to identify risky code patterns and logic flaws as part of the pull request review process.

### Contributor Responsibility

If issues are reported by automated checks, contributors are expected to investigate and resolve them before the pull request can be merged.

### Continuous Improvement

Security checks and validation rules may evolve over time as new best practices emerge.

By submitting a pull request, you agree to participate in this process and help keep the project secure.

## Pull Request Guidelines

Here are a few things you can do to increase the likelihood of your pull request being accepted:

- Follow the existing coding style and project conventions.
- Keep changes focused and self-contained.
- Write tests where appropriate.
- Use clear and descriptive commit messages.
- Submit independent changes as separate pull requests when possible.
- Discuss significant design or architecture changes with maintainers before implementation.