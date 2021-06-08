@Library('miil-shared-libraries@master') _
miilPipeline {
    masterLabel = "cn-cpu" // Node label of machine who will run the pipeline (master)
    slaveLabel = "sh-cpu" // Node label of machines who will run the test workload (slaves)
    toxEnvironments = "py37"
    splitCount = 1

    // (optional) if you want to pass a .env file (environment variables) to the Docker container of microservice
     dotEnvFileCredentialsId = "SESAMIND_TRAIN_DOTENV"
     pythonPackages = '.'
}