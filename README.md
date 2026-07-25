# nil/template

A template repo for new projects

Make sure to do the following:
- rename the following
    - `nil-template` in passed to project cmake method (root CMakeLists.txt)
    - `"nil-template"` in vcpkg.json
- update baseline in vcpkg-configuration.json
- make sure that `config.cmake.in` is proper and complete
    - all external dependencies should be found (find_package)
    - all internal dependencies should be included (include targets.cmake)
- public headers should be in `publish` folder
    - `nil_install_headers()` should be called to register the folder
- all targets should follow the following format:
    - top level project: `nil-(project)`
    - internal targets:
        - `(project)`
        - `(project)-(additional targets)`
- call `nil_install_targets(${PROJECT_NAME})` for targets to be exported/installed