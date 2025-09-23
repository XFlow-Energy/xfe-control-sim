# Contributing to XFLOW-CONTROL-SIM

First off, thank you for considering a contribution! We welcome community involvement to make this project better. To ensure a smooth process for everyone, please review these guidelines.

---
## 1. Development Workflow

1.  **Fork & Branch:** Fork the repository and create a new branch for your feature or bug fix from the `main` branch. Use a descriptive branch name (e.g., `feature/new-pid-controller` or `fix/parser-memory-leak`).
    ```bash
    git checkout -b feature/your-feature-name
    ```

2.  **Develop & Style:** Make your changes. Please adhere to the project's coding style by running `clang-format` and `clang-tidy` before committing.

3.  **Commit Your Changes:** Write clear, descriptive commit messages. If your change addresses an open issue, reference it in the commit body (e.g., `Fixes #42`).
    ```bash
    git commit -m "feat: Add new PID controller logic"
    ```

4.  **Test Your Work:** Ensure the code builds cleanly and all existing tests pass. If you're adding new functionality, please include new tests.

5.  **Submit a Pull Request:** Push your branch and open a pull request against the `main` branch. Provide a clear title and a detailed description of your changes.

---
## 2. License

The XFLOW-CONTROL-SIM project is licensed under the **GNU General Public License v3 (GPLv3)**.

By contributing, you agree that your contributions will be licensed to the project under these same terms.