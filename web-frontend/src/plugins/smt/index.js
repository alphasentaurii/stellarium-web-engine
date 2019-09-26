// Survey Monitoring Tool
// Copyright (c) 2019 - Stellarium Labs SAS
// All rights reserved

import SmtLayerPage from './components/smt-layer-page.vue'
import storeModule from './store'
import alasql from 'alasql'
import Vue from 'vue'
import VueGoogleCharts from 'vue-google-charts'

Vue.use(VueGoogleCharts)

async function loadAllData (fieldsList, jsonData) {
  console.log('Loading ' + jsonData.features.length + ' features')

  console.log('Create Data Base')
  await alasql.promise('CREATE TABLE features (id INT PRIMARY KEY, geometry JSON, properties JSON)')

  // Create an index on each field
  for (let i in fieldsList) {
    let field = fieldsList[i]
    await alasql.promise('CREATE INDEX idx_' + i + ' ON features(' + field.id.replace(/\./g, '->') + ')')
  }

  // Insert all data
  let req = alasql.compile('INSERT INTO features VALUES (?, ?, ?)')
  for (let feature of jsonData.features) {
    let id = feature.id
    let geometry = feature.geometry
    let properties = feature.properties
    await req.promise([id, geometry, properties])
  }

  // console.log('Test query by properties.scenario.contractual_date')
  // let res = await alasql.promise('SELECT * FROM features WHERE DATE(properties->scenario->contractual_date) > DATE("2022-04-05")')
  // console.log(res)
}

export default {
  vuePlugin: {
    install: (Vue, options) => {
      Vue.component('smt-layer-page', SmtLayerPage)
    }
  },
  name: 'SMT',
  storeModule: storeModule,
  panelRoutes: [
    { path: '/p/smt', component: SmtLayerPage, meta: { tabName: 'Survey Tool', prio: 2 } }
  ],
  onEngineReady: function (app) {
    app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'initializing' })

    // Init base view settings
    app.$stel.core.lines.equatorial.visible = true
    app.$stel.core.atmosphere.visible = false
    app.$stel.core.landscapes.visible = false
    app.$stel.core.landscapes.fog_visible = false
    app.$stel.core.cardinals.visible = false
    app.$stel.core.planets.visible = false
    app.$stel.core.mount_frame = app.$stel.FRAME_ICRF
    app.$stel.core.projection = 4 // Hammer
    app.$stel.core.observer.refraction = false
    app.$stel.core.observer.yaw = 0
    app.$stel.core.observer.pitch = 0
    app.$stel.core.fov = 270

    let fieldsList = require('./fieldsList.json')
    Vue.prototype.$smt = { fieldsList: fieldsList }

    app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'loading' })
    let jsonData = require('./data/euclid-test.json')
    loadAllData(fieldsList, jsonData).then(_ => {
      app.$store.commit('setValue', { varName: 'SMT.status', newValue: 'ready' })
    })
  }
}
