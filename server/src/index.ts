import express, { ErrorRequestHandler } from 'express'
import { LISTEN_PORT } from './config'
import { logger } from './log'
import { calculateResponse, getRandomChallenge } from './puf'
import { AuthRequest, AuthResponse, HelloRequest, HelloResponse } from './types/api'
import { PufRequests, PufTable } from './types/puf'

const pufTable: PufTable = {}

const pufRequests: PufRequests = {}

function serve() {
  const app = express()

  app.use(express.json())

  // middleware for logging
  app.use((req, _, next) => {
    logger.debug(`request [${req.method}]: ${req.url}`)
    next()
  })

  // error handler
  app.use(((err, req, _res, next) => {
    logger.error(`error handling ${req.method} ${req.url}`)
    next(err)
  }) as ErrorRequestHandler)

  app.post('/puf/hello', (req, res) => {
    const helloRequest = req.body as HelloRequest

    if (!(helloRequest.deviceId in pufTable)) {
      res.status(400)
      return
    }

    const challenge = getRandomChallenge()
    const response = calculateResponse(pufTable[helloRequest.deviceId], challenge)

    pufRequests[helloRequest.deviceId] = { challenge, response }

    const helloResponse: HelloResponse = { challenge }
    res.send(helloResponse)
  })

  app.post('/puf/auth', (req, res) => {
    const authRequest = req.body as AuthRequest

    if (!(authRequest.deviceId in pufRequests)) {
      res.status(400)
      return
    }

    let authenticated = false

    if (authRequest.response === pufRequests[authRequest.deviceId].response) {
      authenticated = true
      delete pufRequests[authRequest.deviceId]
    }

    const authResponse: AuthResponse = { success: authenticated }
    res.send(authResponse)
  })

  app.listen(LISTEN_PORT, () => logger.info(`Server listening on port ${LISTEN_PORT}!`))
}

serve()
