// this is automatically generated template header please implement and edit.

#include "anomaly_serv.hpp"
#include "../anomaly/anomaly_factory.hpp"
#include "../fv_converter/converter_config.hpp"

#include "anomaly_client.hpp"

using namespace jubatus::framework;

namespace jubatus { namespace server { // do not change
anomaly_serv::anomaly_serv(const server_argv& a)
  :framework::jubatus_serv(a)
{
  std::map<std::string,std::string>  config;
  config["anomaly:name"] = "lof";
  config["name"] = "euclid_lsh";
  anomaly::anomaly_base* an = anomaly::create_anomaly(config);
  anomaly_.set_model(common::cshared_ptr<anomaly::anomaly_base>(an));
  register_mixable(framework::mixable_cast(&anomaly_));

  wm_.wm_ = common::cshared_ptr<fv_converter::weight_manager>(new weight_manager);
  wm_.set_model(wm_.wm_);
  register_mixable(framework::mixable_cast(&wm_));
}

anomaly_serv::~anomaly_serv()
{}


//update, broadcast
bool anomaly_serv::set_config(const config_data& c)
{
  config_ = c;
  
  common::cshared_ptr<fv_converter::datum_to_fv_converter>
    converter(new fv_converter::datum_to_fv_converter);
    
  jubatus::fv_converter::converter_config converter_config;
  convert(c.converter, converter_config);
  fv_converter::initialize_converter(converter_config, *converter);
  converter_ = converter;

  wm_.wm_ = common::cshared_ptr<fv_converter::weight_manager>(new weight_manager);
  wm_.set_model(wm_.wm_);

  anomaly::anomaly_base* an = anomaly::create_anomaly(c.config);
  anomaly_.set_model(common::cshared_ptr<anomaly::anomaly_base>(an));
  return true;
}

//analysis, random
config_data anomaly_serv::get_config() const
{
  return config_;
}

//update, cht
bool anomaly_serv::clear_row(const std::string& id)
{
  anomaly_.get_model()->clear_row(id);
  return true;
}

//update, random
std::pair<std::string,float > anomaly_serv::add(const datum& d)
{
  uint64_t id = idgen_.generate();
  std::string id_str = pfi::lang::lexical_cast<std::string>(id);

  if(a_.is_standalone()){
    
    float score = update(id_str, d);
    return make_pair(id_str, score);
    
  }else{

    std::vector<std::pair<std::string, int> > nodes;
    float score = 0;
    find_from_cht(id_str, 2, nodes);
    if(nodes.empty()){
      throw std::runtime_error("fatal: no server found in cht: "+id_str);
    }
    // this sequences MUST success, in case of failures the whole request should be canceled
    if(nodes[0].first == a_.eth && nodes[0].second == a_.port){
      score = this->update(id_str, d);
    }else{
      client::anomaly c(nodes[0].first, nodes[0].second, 5.0);
      score = c.update(a_.name, id_str, d);
    }
    
    for(size_t i = 1; i < nodes.size(); ++i){
      try{
	if(nodes[i].first == a_.eth && nodes[i].second == a_.port){
	  score = this->update(id_str, d);
	}else{
	  client::anomaly c(nodes[i].first, nodes[i].second, 5.0);
	  score = c.update(a_.name, id_str, d);
	}
	
      }catch(const std::runtime_error& e){ // error !
	LOG(INFO) << i+1 << "th replica: " << nodes[i].first << ":" << nodes[i].second << " " << e.what();
      }
    }
    return make_pair(id_str, score);
  }
}

//update, cht
float anomaly_serv::update(const std::string& id, const datum& d)
{
  fv_converter::datum data;
  sfv_t v;
  convert(d, data);
  converter_->convert(data, v);
  anomaly_.get_model()->update_row(id, v);
  float score = anomaly_.get_model()->calc_anomaly_score(id);
  return score;
}

//update, broadcast
bool anomaly_serv::clear()
{
  anomaly_.get_model()->clear();
  return true;
}

//analysis, random
float anomaly_serv::calc_score(const datum& d) const
{
  fv_converter::datum data;
  sfv_t v;
  convert(d, data);
  converter_->convert(data, v);
  float score = anomaly_.get_model()->calc_anomaly_score(v);
  return score;
}

//analysis, broadcast
std::vector<std::string > anomaly_serv::get_all_rows() const
{
  std::vector<std::string> ids;
  anomaly_.get_model()->get_all_row_ids(ids);
  return ids;
}

void anomaly_serv::after_load(){}
}} // namespace jubatus::server
