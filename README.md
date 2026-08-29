# EVA Driver

EVA Driver provides kernel driver support for the Qualcomm® Engine for Visual Analytics (EVA) subsystem on Qualcomm platforms. The driver enables communication between the Linux host and EVA hardware components and provides the infrastructure required by user space applications and associated software stacks.

## Branches

**eva-kernel.qclinux.0.0**: Primary development branch. Contributors should develop submissions based on this branch and submit pull requests targeting this branch.

## Requirements

- Linux development environment
- Supported Qualcomm platform with EVA hardware support
- Linux kernel sources compatible with the target branch
- Git
- Standard Linux kernel build dependencies

## Installation Instructions

1. Clone the repository.
2. Checkout the appropriate development branch.
3. Integrate the driver into the target kernel source tree.
4. Build the kernel and kernel modules.
5. Deploy the generated image and modules to the target platform.

Refer to platform-specific build documentation for detailed build and deployment instructions.

## Usage

The EVA driver is intended to be used as part of the Qualcomm Linux software stack. Once the driver is loaded and initialized, applications and middleware can communicate with the EVA subsystem through the exposed kernel interfaces.

## Development

Contributions are welcome through GitHub pull requests.

1. Fork the repository.
2. Create a topic branch for your changes.
3. Commit changes with appropriate commit messages and sign-offs.
4. Submit a pull request for review.

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed contribution guidelines.

## Getting in Contact

For project questions, bug reports, and feature requests:

- ../../issues

For security-related issues, please refer to [SECURITY.md](SECURITY.md).

## License

EVA Driver is licensed under the license specified in [LICENSE.txt](LICENSE.txt). See the license file for complete licensing terms.