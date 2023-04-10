import dotenv from 'dotenv'
import { TLogLevelName } from 'tslog'

dotenv.config()

export const LOG_LEVEL = (process.env.LOG_LEVEL as TLogLevelName) ?? 'info'
export const CSV_OUT_DIR = process.env.CSV_OUT_DIR ?? 'out'
export const LISTEN_PORT = process.env.LISTEN_PORT ?? 3000
