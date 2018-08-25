module.exports = function (grunt) {

    require('load-grunt-tasks')(grunt);
    var pkg = grunt.file.readJSON('package.json');

    // Project configuration
    grunt.initConfig({

        pkg: pkg,
        cssmin: {
            master: {
                files: {
                    'fs/wifi_portal.min.css': 'fs/wifi_portal.css'
                }
            }
        },
        uglify: {
            master: {
                options: {
                    preserveComments: false,
                    compress: {
                        drop_console: true,
                        global_defs: {
                            "DEBUG": false
                        }
                    },
                },
                files: {
                    'fs/wifi_portal.min.js': 'fs/wifi_portal.js'
                }
            }
        },
        copy: {
            source: {
                expand: true,
                cwd: 'fs/',
                src: ['wifi_portal.html', 'wifi_portal.js', 'wifi_portal.css'],
                dest: 'portal_src/',
                flatten: true
            },
            html: {
                src: ['fs/wifi_portal.html'],
                dest: 'fs/wifi_portal.html.tmp',
                options: {
                    process: function (content, srcpath) {
                        // Update CSS and JS files to gzip versions
                        return content.replace('wifi_portal.js', 'wifi_portal.min.js.gz').replace('wifi_portal.css', 'wifi_portal.min.css.gz');
                    },
                }
            }
        },
        clean: {
            source: ['fs/wifi_portal.css', 'fs/wifi_portal.js', 'fs/wifi_portal.html', 'fs/wifi_portal.min.html', 'fs/wifi_portal.html.tmp'],
            min: ['fs/wifi_portal.min.css', 'fs/wifi_portal.min.js', 'fs/wifi_portal.html']
        },
        htmlmin: {
            master: {
                options: {
                    removeComments: true,
                    collapseWhitespace: true
                },
                files: {
                    'fs/wifi_portal.min.html': 'fs/wifi_portal.html.tmp'
                }
            }
        },
        compress: {
            gzip: {
                options: {
                    mode: 'gzip'
                },
                files: {
                    'fs/wifi_portal.min.html.gz': 'fs/wifi_portal.min.html',
                    'fs/wifi_portal.min.css.gz': 'fs/wifi_portal.min.css',
                    'fs/wifi_portal.min.js.gz': 'fs/wifi_portal.min.js',
                }
            }
        }
    });

    // Build for master branch
    grunt.registerTask('master', ['copy:source', 'copy:html', 'htmlmin:master', 'cssmin:master', 'uglify:master', 'compress:gzip', 'clean:min', 'clean:source']);
    grunt.registerTask('master-noclean', ['copy:source', 'copy:html', 'htmlmin:master', 'cssmin:master', 'uglify:master', 'compress:gzip']);

    //grunt.util.linefeed = '\n';
};