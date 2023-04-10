import { Logger } from 'tslog'
import { LOG_LEVEL } from './config'

export const logger = new Logger({ minLevel: LOG_LEVEL })
