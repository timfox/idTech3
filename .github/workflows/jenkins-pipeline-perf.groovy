// Jenkins Pipeline with Performance Regression Testing
// This demonstrates how to integrate the performance test system into Jenkins CI/CD

pipeline {
    agent any

    parameters {
        choice(name: 'TEST_TYPE', choices: ['full', 'smoke', 'targeted'], description: 'Type of performance tests to run')
        booleanParam(name: 'UPDATE_BASELINES', defaultValue: false, description: 'Update performance baselines')
        string(name: 'TARGET_BRANCH', defaultValue: 'main', description: 'Branch to compare performance against')
    }

    environment {
        PERF_BASELINE_DIR = 'performance-baselines'
        PERF_RESULTS_DIR = 'performance-results'
        BUILD_TYPE = 'Release'
    }

    stages {
        stage('Checkout') {
            steps {
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: '*/${BRANCH_NAME}']],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [[
                        $class: 'CloneOption',
                        depth: 0,
                        noTags: false,
                        reference: '',
                        shallow: false
                    ]],
                    userRemoteConfigs: [[
                        credentialsId: 'git-credentials',
                        url: scm.userRemoteConfigs[0].url
                    ]]
                ])
            }
        }

        stage('Setup Build Environment') {
            steps {
                sh '''
                    # Install dependencies
                    if command -v apt-get >/dev/null 2>&1; then
                        sudo apt-get update
                        sudo apt-get install -y build-essential cmake libsdl2-dev libvulkan-dev
                    elif command -v yum >/dev/null 2>&1; then
                        sudo yum install -y gcc gcc-c++ make cmake SDL2-devel vulkan-devel
                    fi
                '''
            }
        }

        stage('Build') {
            steps {
                sh '''
                    mkdir -p build
                    cd build
                    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DUSE_PERF_TESTS=ON
                    make -j$(nproc)
                '''
            }
        }

        stage('Load Performance Baselines') {
            steps {
                sh '''
                    # Download or copy baseline data from previous successful builds
                    if [ -d "${PERF_BASELINE_DIR}" ]; then
                        echo "Loading existing performance baselines..."
                        ./build/idtech3_vulkan_x86_64 +set perftest baseline load ${PERF_BASELINE_DIR}/baselines.json
                    else
                        echo "No existing baselines found, will create new ones"
                    fi
                '''
            }
        }

        stage('Run Performance Tests') {
            steps {
                script {
                    def testConfigs = [:]

                    if (params.TEST_TYPE == 'full') {
                        testConfigs = [
                            'basic_rendering': [duration: 60, warmup: 10],
                            'memory_stress': [duration: 45, warmup: 5],
                            'asset_loading': [duration: 30, warmup: 5],
                            'physics_simulation': [duration: 45, warmup: 8],
                            'network_stress': [duration: 30, warmup: 3],
                            'multithreaded_rendering': [duration: 40, warmup: 7]
                        ]
                    } else if (params.TEST_TYPE == 'smoke') {
                        testConfigs = [
                            'basic_rendering': [duration: 15, warmup: 3],
                            'memory_stress': [duration: 10, warmup: 2]
                        ]
                    } else if (params.TEST_TYPE == 'targeted') {
                        testConfigs = [
                            'basic_rendering': [duration: 30, warmup: 5]
                        ]
                    }

                    // Create test suite
                    sh "./build/idtech3_vulkan_x86_64 +set perftest suite create ci_suite 'Jenkins CI Performance Test Suite'"

                    // Add tests to suite
                    testConfigs.each { testName, config ->
                        sh "./build/idtech3_vulkan_x86_64 +set perftest suite add ci_suite ${testName} ${config.duration} ${config.warmup}"
                    }

                    // Run the test suite
                    def testResult = sh(
                        script: "./build/idtech3_vulkan_x86_64 +set perftest suite run ci_suite",
                        returnStatus: true
                    )

                    if (testResult != 0) {
                        currentBuild.result = 'UNSTABLE'
                        echo "Performance tests completed with issues"
                    }
                }
            }
        }

        stage('Check Performance Regressions') {
            steps {
                script {
                    def regressionCheck = sh(
                        script: './build/idtech3_vulkan_x86_64 +set perftest stats | grep "Performance Regressions:" | awk \'{print $3}\'',
                        returnStdout: true
                    ).trim()

                    def regressionCount = regressionCheck.isInteger() ? regressionCheck.toInteger() : 0

                    if (regressionCount > 0) {
                        currentBuild.result = 'FAILURE'
                        echo "PERFORMANCE REGRESSION DETECTED: ${regressionCount} regression(s) found"

                        // Create regression report
                        sh "./build/idtech3_vulkan_x86_64 +set perftest report regression_report.json"

                        // Archive regression details
                        archiveArtifacts artifacts: 'regression_report.json', fingerprint: true

                        // Notify team
                        emailext(
                            subject: "Performance Regression Detected - Build #${env.BUILD_NUMBER}",
                            body: """
                                Performance regression detected in build #${env.BUILD_NUMBER}

                                Branch: ${env.BRANCH_NAME}
                                Commit: ${env.GIT_COMMIT}
                                Regressions: ${regressionCount}

                                Check the attached regression report for details.

                                Build URL: ${env.BUILD_URL}
                            """,
                            attachmentsPattern: 'regression_report.json',
                            to: 'dev-team@company.com'
                        )

                        error("Performance regression detected - failing build")
                    } else {
                        echo "No performance regressions detected"
                    }
                }
            }
        }

        stage('Generate Performance Reports') {
            steps {
                sh '''
                    mkdir -p ${PERF_RESULTS_DIR}

                    # Generate comprehensive performance report
                    ./build/idtech3_vulkan_x86_64 +set perftest report ${PERF_RESULTS_DIR}/performance_report.json

                    # Export results for CI consumption
                    ./build/idtech3_vulkan_x86_64 +set perftest ci export ${PERF_RESULTS_DIR}

                    # Generate HTML dashboard
                    ./build/idtech3_vulkan_x86_64 +set perftest dashboard generate ${PERF_RESULTS_DIR}/dashboard.html
                '''

                archiveArtifacts artifacts: '${PERF_RESULTS_DIR}/**', fingerprint: true
            }
        }

        stage('Performance Trend Analysis') {
            steps {
                sh '''
                    # Compare with previous builds
                    if [ -f "previous_build_results.json" ]; then
                        echo "Comparing with previous build performance..."
                        # In a real implementation, this would use the performance test system
                        # to compare results and identify trends
                    fi
                '''
            }
        }

        stage('Update Baselines') {
            when {
                allOf {
                    expression { params.UPDATE_BASELINES }
                    anyOf {
                        branch 'main'
                        branch 'master'
                    }
                    expression { currentBuild.result == null || currentBuild.result == 'SUCCESS' }
                }
            }
            steps {
                sh '''
                    echo "Updating performance baselines..."

                    # Save new baselines
                    ./build/idtech3_vulkan_x86_64 +set perftest baseline save ${PERF_BASELINE_DIR}/baselines_new.json

                    # Archive baselines for future builds
                    mkdir -p baseline_archive
                    cp ${PERF_BASELINE_DIR}/baselines_new.json baseline_archive/baselines_${BUILD_NUMBER}.json
                '''

                archiveArtifacts artifacts: 'baseline_archive/baselines_*.json', fingerprint: true
            }
        }

        stage('Publish Results') {
            steps {
                script {
                    // Publish test results to Jenkins
                    perfpublisher(
                        healthy: 80,
                        unhealthy: 20,
                        metrics: [
                            [$class: 'PercentageOfDurationMetric', name: 'Frame Time'],
                            [$class: 'PercentageOfDurationMetric', name: 'FPS'],
                            [$class: 'PercentageOfDurationMetric', name: 'Memory Usage'],
                            [$class: 'PercentageOfDurationMetric', name: 'CPU Usage']
                        ]
                    )

                    // Publish HTML dashboard
                    publishHTML([
                        allowMissing: false,
                        alwaysLinkToLastBuild: true,
                        keepAll: true,
                        reportDir: PERF_RESULTS_DIR,
                        reportFiles: 'dashboard.html',
                        reportName: 'Performance Dashboard'
                    ])
                }
            }
        }
    }

    post {
        always {
            // Clean up and archive logs
            sh '''
                ./build/idtech3_vulkan_x86_64 +set perftest stats > performance_stats.log
                ./build/idtech3_vulkan_x86_64 +set perftest baseline list > baseline_info.log
            '''

            archiveArtifacts artifacts: '*.log', fingerprint: true

            // Send summary email
            emailext(
                subject: "Performance Test Results - Build #${env.BUILD_NUMBER} - ${currentBuild.result}",
                body: """
                    Performance Test Summary for Build #${env.BUILD_NUMBER}

                    Branch: ${env.BRANCH_NAME}
                    Result: ${currentBuild.result}
                    Test Type: ${params.TEST_TYPE}

                    Build URL: ${env.BUILD_URL}

                    Check the attached logs and archived artifacts for detailed results.
                """,
                attachmentsPattern: '*.log',
                to: 'qa-team@company.com'
            )
        }

        failure {
            // Additional failure handling
            script {
                echo "Performance tests failed - check artifacts for details"

                // Could trigger rollback or other automated responses here
            }
        }

        success {
            script {
                if (params.UPDATE_BASELINES && (env.BRANCH_NAME == 'main' || env.BRANCH_NAME == 'master')) {
                    echo "Baselines updated successfully for main branch"
                }
            }
        }
    }

    options {
        timeout(time: 2, unit: 'HOURS')
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr: '50'))
    }

    triggers {
        // Run performance tests nightly
        cron('H 2 * * *')

        // Run on significant commits
        pollSCM('H/30 * * * *')
    }
}
